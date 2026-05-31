/*
 * Copyright (c) Tony Bybell 1999-2016.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <config.h>
#include "globals.h"
#include "pixmaps.h"
#include "currenttime.h"
#include "debug.h"

void fix_wavehadj(void)
{
    GtkAdjustment *hadj;
    gfloat pageinc;

    hadj = GTK_ADJUSTMENT(GLOBALS->wave_hslider);
    gtk_adjustment_set_lower(hadj, GLOBALS->tims.first);
    gtk_adjustment_set_upper(hadj, GLOBALS->tims.last + 2.0);

    pageinc = (gfloat)(((gdouble)GLOBALS->wavewidth) * GLOBALS->nspx);
    gtk_adjustment_set_page_increment(hadj, (pageinc >= 1.0) ? pageinc : 1.0);
    gtk_adjustment_set_page_size(hadj, gtk_adjustment_get_page_increment(hadj));

    /* hadj->step_increment=(GLOBALS->nspx>=1.0)?GLOBALS->nspx:1.0; */

    gtk_adjustment_set_step_increment(hadj, pageinc / 10.0);
    if (gtk_adjustment_get_step_increment(hadj) < 1.0)
        gtk_adjustment_set_step_increment(hadj, 1.0);

    gtk_adjustment_set_value(hadj, GLOBALS->tims.start); /* work around GTK3 clamping code */

    if (gtk_adjustment_get_page_size(hadj) >=
        (gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_lower(hadj)))
        gtk_adjustment_set_value(hadj, gtk_adjustment_get_lower(hadj));
    if (gtk_adjustment_get_value(hadj) + gtk_adjustment_get_page_size(hadj) >
        gtk_adjustment_get_upper(hadj)) {
        gtk_adjustment_set_value(hadj,
                                 gtk_adjustment_get_upper(hadj) -
                                     gtk_adjustment_get_page_size(hadj));
        if (gtk_adjustment_get_value(hadj) < gtk_adjustment_get_lower(hadj))
            gtk_adjustment_set_value(hadj, gtk_adjustment_get_lower(hadj));
    }
}

/* ====================================================================
 * nipscern: animated zoom (acceleration/deceleration easing).
 *
 * Zoom in/out used to jump the zoom level by one step instantly. Instead we
 * interpolate the (fractional) zoom from its current value to the target over
 * a short duration using an ease-out curve, driven by the widget frame clock
 * so it tracks the monitor refresh (smooth at high fps). Each frame we apply
 * the intermediate zoom and force a waveform redraw.
 *
 * Honest limitation: the waveform is CPU/Cairo rendered, so the achievable fps
 * depends on the trace size - this is smooth on typical traces but cannot be
 * guaranteed at 120 fps on very large ones.
 * ==================================================================== */

static gboolean gw_zoom_animate_enabled = TRUE;
static const gint64 GW_ZOOM_ANIM_DURATION_US = 150000; /* 150 ms */

static struct
{
    gboolean active;
    gdouble from;
    gdouble to;
    gdouble current;
    GwTime middle;
    gboolean do_center;
    gint64 start_us;
    guint tick_id;
    GtkWidget *widget;
} gw_zoom_anim;

static gdouble gw_ease_out_cubic(gdouble t)
{
    gdouble u = 1.0 - t;
    return 1.0 - (u * u * u);
}

/* Apply a (possibly fractional) zoom level, re-centering the viewport on
 * `middle`, then force the waveform/scrollbar to refresh. */
static void gw_zoom_apply(gdouble zoom, GwTime middle, gboolean do_center)
{
    calczoom(zoom);

    if (do_center) {
        GwTime width = (GwTime)(((gdouble)GLOBALS->wavewidth) * GLOBALS->nspx);
        GLOBALS->tims.start = time_trunc(middle - (width / 2));
        if (GLOBALS->tims.start + width > GLOBALS->tims.last) {
            GLOBALS->tims.start = time_trunc(GLOBALS->tims.last - width);
        }
        if (GLOBALS->tims.start < GLOBALS->tims.first) {
            GLOBALS->tims.start = GLOBALS->tims.first;
        }
        gtk_adjustment_set_value(GTK_ADJUSTMENT(GLOBALS->wave_hslider),
                                 GLOBALS->tims.timecache = GLOBALS->tims.start);
    } else {
        GLOBALS->tims.timecache = 0;
    }

    fix_wavehadj();
    g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider), "changed");
    g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider), "value_changed");
}

static gboolean gw_zoom_anim_tick(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer user_data)
{
    (void)widget;
    (void)user_data;

    gint64 now = gdk_frame_clock_get_frame_time(frame_clock);
    gdouble t = (gdouble)(now - gw_zoom_anim.start_us) / (gdouble)GW_ZOOM_ANIM_DURATION_US;

    if (t >= 1.0) {
        gw_zoom_anim.current = gw_zoom_anim.to;
        GLOBALS->tims.zoom = gw_zoom_anim.to;
        gw_zoom_apply(gw_zoom_anim.to, gw_zoom_anim.middle, gw_zoom_anim.do_center);
        gw_zoom_anim.active = FALSE;
        gw_zoom_anim.tick_id = 0;
        return G_SOURCE_REMOVE;
    }

    gdouble eased = gw_ease_out_cubic(t);
    gdouble z = gw_zoom_anim.from + (gw_zoom_anim.to - gw_zoom_anim.from) * eased;
    gw_zoom_anim.current = z;
    GLOBALS->tims.zoom = z;
    gw_zoom_apply(z, gw_zoom_anim.middle, gw_zoom_anim.do_center);
    return G_SOURCE_CONTINUE;
}

/* Animate the zoom to `target`, centering on `middle`. Falls back to an
 * instant apply when animation is disabled or no realized window exists. */
static void gw_zoom_animate_to(gdouble target, GwTime middle, gboolean do_center)
{
    gdouble from = gw_zoom_anim.active ? gw_zoom_anim.current : (gdouble)GLOBALS->tims.zoom;
    GtkWidget *widget = GLOBALS->mainwindow;

    if (!gw_zoom_animate_enabled || widget == NULL || !gtk_widget_get_realized(widget)) {
        GLOBALS->tims.zoom = target;
        gw_zoom_apply(target, middle, do_center);
        return;
    }

    if (gw_zoom_anim.active && gw_zoom_anim.tick_id != 0 && gw_zoom_anim.widget != NULL) {
        gtk_widget_remove_tick_callback(gw_zoom_anim.widget, gw_zoom_anim.tick_id);
    }

    GdkFrameClock *clock = gtk_widget_get_frame_clock(widget);

    gw_zoom_anim.active = TRUE;
    gw_zoom_anim.from = from;
    gw_zoom_anim.to = target;
    gw_zoom_anim.current = from;
    gw_zoom_anim.middle = middle;
    gw_zoom_anim.do_center = do_center;
    gw_zoom_anim.widget = widget;
    gw_zoom_anim.start_us =
        (clock != NULL) ? gdk_frame_clock_get_frame_time(clock) : g_get_monotonic_time();
    gw_zoom_anim.tick_id = gtk_widget_add_tick_callback(widget, gw_zoom_anim_tick, NULL, NULL);
}

/* Compute the center to keep fixed for a zoom step, honouring do_zoom_center
 * and the primary marker (mirrors the original zoom in/out logic). */
static GwTime gw_zoom_center_point(void)
{
    GwMarker *primary_marker = gw_project_get_primary_marker(GLOBALS->project);
    GwTime primary_pos = gw_marker_get_position(primary_marker);

    if (!gw_marker_is_enabled(primary_marker) || primary_pos < GLOBALS->tims.first ||
        primary_pos > GLOBALS->tims.last) {
        if (GLOBALS->tims.end > GLOBALS->tims.last) {
            GLOBALS->tims.end = GLOBALS->tims.last;
        }
        GwTime middle = (GLOBALS->tims.start / 2) + (GLOBALS->tims.end / 2);
        if ((GLOBALS->tims.start & 1) && (GLOBALS->tims.end & 1)) {
            middle++;
        }
        return middle;
    }

    return primary_pos;
}

void service_zoom_left(GtkWidget *text, gpointer data)
{
    (void)text;
    (void)data;

    GtkAdjustment *hadj;

    hadj = GTK_ADJUSTMENT(GLOBALS->wave_hslider);
    gtk_adjustment_set_value(hadj, GLOBALS->tims.timecache = GLOBALS->tims.first);
    time_update();
}

void service_zoom_right(GtkWidget *text, gpointer data)
{
    (void)text;
    (void)data;

    GtkAdjustment *hadj;
    GwTime ntinc;

    ntinc = (GwTime)(((gdouble)GLOBALS->wavewidth) * GLOBALS->nspx);

    GLOBALS->tims.timecache = GLOBALS->tims.last - ntinc + 1;
    if (GLOBALS->tims.timecache < GLOBALS->tims.first)
        GLOBALS->tims.timecache = GLOBALS->tims.first;

    hadj = GTK_ADJUSTMENT(GLOBALS->wave_hslider);
    gtk_adjustment_set_value(hadj, GLOBALS->tims.timecache);
    time_update();
}

void service_zoom_out(GtkWidget *text, gpointer data)
{
    (void)text;
    (void)data;

    gboolean do_center = GLOBALS->do_zoom_center;
    GwTime middle = do_center ? gw_zoom_center_point() : 0;

    /* base off the in-flight target (if any) so rapid clicks chain cleanly */
    gdouble base = gw_zoom_anim.active ? gw_zoom_anim.to : (gdouble)GLOBALS->tims.zoom;
    GLOBALS->tims.prevzoom = GLOBALS->tims.zoom;

    gw_zoom_animate_to(base - 1.0, middle, do_center);

    DEBUG(printf("Zoombuttons out\n"));
}

void service_zoom_in(GtkWidget *text, gpointer data)
{
    (void)text;
    (void)data;

    /* base off the in-flight target (if any) so rapid clicks chain cleanly */
    gdouble base = gw_zoom_anim.active ? gw_zoom_anim.to : (gdouble)GLOBALS->tims.zoom;

    if (base < 0) /* otherwise it's ridiculous and can cause overflow in the scope */
    {
        gboolean do_center = GLOBALS->do_zoom_center;
        GwTime middle = do_center ? gw_zoom_center_point() : 0;

        GLOBALS->tims.prevzoom = GLOBALS->tims.zoom;

        gw_zoom_animate_to(base + 1.0, middle, do_center);

        DEBUG(printf("Zoombuttons in\n"));
    }
}

void service_zoom_undo(GtkWidget *text, gpointer data)
{
    (void)text;
    (void)data;

    gdouble temp;

    temp = GLOBALS->tims.zoom;
    GLOBALS->tims.zoom = GLOBALS->tims.prevzoom;
    GLOBALS->tims.prevzoom = temp;
    GLOBALS->tims.timecache = 0;
    calczoom(GLOBALS->tims.zoom);
    fix_wavehadj();

    g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider), "changed"); /* force zoom update */
    g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider),
                          "value_changed"); /* force zoom update */

    DEBUG(printf("Zoombuttons Undo\n"));
}

void service_zoom_fit(GtkWidget *text, gpointer data)
{
    (void)text;
    (void)data;

    gdouble estimated;
    int fixedwidth;

    GwMarker *primary_marker = gw_project_get_primary_marker(GLOBALS->project);
    GwMarker *baseline_marker = gw_project_get_baseline_marker(GLOBALS->project);

    if (gw_marker_is_enabled(baseline_marker) && gw_marker_is_enabled(primary_marker)) {
        /* new semantics added to zoom between the two */
        service_dragzoom(gw_marker_get_position(baseline_marker),
                         gw_marker_get_position(primary_marker));
    } else {
        if (GLOBALS->wavewidth > 4) {
            fixedwidth = GLOBALS->wavewidth - 4;
        } else {
            fixedwidth = GLOBALS->wavewidth;
        }
        estimated = -log(((gdouble)(GLOBALS->tims.last - GLOBALS->tims.first + 1)) /
                         ((gdouble)fixedwidth) * ((gdouble)200.0)) /
                    log(GLOBALS->zoombase);
        if (estimated > ((gdouble)(0.0)))
            estimated = ((gdouble)(0.0));

        GLOBALS->tims.prevzoom = GLOBALS->tims.zoom;
        GLOBALS->tims.timecache = 0;

        calczoom(estimated);
        GLOBALS->tims.zoom = estimated;

        fix_wavehadj();

        g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider),
                              "changed"); /* force zoom update */
        g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider),
                              "value_changed"); /* force zoom update */
    }

    DEBUG(printf("Zoombuttons Fit\n"));
}

void service_zoom_full(GtkWidget *text, gpointer data)
{
    (void)text;
    (void)data;

    gdouble estimated;
    int fixedwidth;

    if (GLOBALS->wavewidth > 4) {
        fixedwidth = GLOBALS->wavewidth - 4;
    } else {
        fixedwidth = GLOBALS->wavewidth;
    }
    estimated = -log(((gdouble)(GLOBALS->tims.last - GLOBALS->tims.first + 1)) /
                     ((gdouble)fixedwidth) * ((gdouble)200.0)) /
                log(GLOBALS->zoombase);
    if (estimated > ((gdouble)(0.0)))
        estimated = ((gdouble)(0.0));

    GLOBALS->tims.prevzoom = GLOBALS->tims.zoom;
    GLOBALS->tims.timecache = 0;

    calczoom(estimated);
    GLOBALS->tims.zoom = estimated;

    fix_wavehadj();

    g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider), "changed"); /* force zoom update */
    g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider),
                          "value_changed"); /* force zoom update */

    DEBUG(printf("Zoombuttons Full\n"));
}

void service_dragzoom(GwTime time1, GwTime time2) /* the function you've been waiting for... */
{
    gdouble estimated;
    int fixedwidth;
    GwTime temp;
    GtkAdjustment *hadj;
    GwTrace *t;
    int dragzoom_ok = 1;

    if (time2 < time1) {
        temp = time1;
        time1 = time2;
        time2 = temp;
    }

    if (GLOBALS->dragzoom_threshold) {
        GwTime tdelta = time2 - time1;
        gdouble x = tdelta * GLOBALS->pxns;
        if (x < GLOBALS->dragzoom_threshold) {
            dragzoom_ok = 0;
        }
    }

    if ((time2 > time1) && (dragzoom_ok)) /* ensure at least 1 tick and dragzoom_threshold if set */
    {
        if (GLOBALS->wavewidth > 4) {
            fixedwidth = GLOBALS->wavewidth - 4;
        } else {
            fixedwidth = GLOBALS->wavewidth;
        }
        estimated =
            -log(((gdouble)(time2 - time1 + 1)) / ((gdouble)fixedwidth) * ((gdouble)200.0)) /
            log(GLOBALS->zoombase);
        if (estimated > ((gdouble)(0.0)))
            estimated = ((gdouble)(0.0));

        GLOBALS->tims.prevzoom = GLOBALS->tims.zoom;
        GLOBALS->tims.timecache = GLOBALS->tims.laststart = GLOBALS->tims.start = time_trunc(time1);

        for (t = GLOBALS->traces.first; t;
             t = t->t_next) /* have to nuke string refs so printout is ok! */
        {
            if (t->asciivalue) {
                free_2(t->asciivalue);
                t->asciivalue = NULL;
            }
        }

        for (t = GLOBALS->traces.buffer; t; t = t->t_next) {
            if (t->asciivalue) {
                free_2(t->asciivalue);
                t->asciivalue = NULL;
            }
        }

        GwMarker *primary_marker = gw_project_get_primary_marker(GLOBALS->project);
        GwMarker *baseline_marker = gw_project_get_baseline_marker(GLOBALS->project);

        if (!(gw_marker_is_enabled(baseline_marker) && gw_marker_is_enabled(primary_marker))) {
            gw_marker_set_enabled(primary_marker, FALSE);
            update_time_box();
        }
        GLOBALS->signalwindow_width_dirty = 1;
        MaxSignalLength();

        hadj = GTK_ADJUSTMENT(GLOBALS->wave_hslider);
        gtk_adjustment_set_value(hadj, time1);

        calczoom(estimated);
        GLOBALS->tims.zoom = estimated;

        fix_wavehadj();

        g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider),
                              "changed"); /* force zoom update */
        g_signal_emit_by_name(GTK_ADJUSTMENT(GLOBALS->wave_hslider),
                              "value_changed"); /* force zoom update */

        DEBUG(printf("Drag Zoom\n"));
    }
}
