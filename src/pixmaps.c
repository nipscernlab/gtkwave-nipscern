/*
 * Copyright (c) Tony Bybell 1999-2012.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "globals.h"
#include <config.h>
#include "pixmaps.h"

/* nipscern: hierarchy icons are modern Phosphor SVGs, pre-recolored for the
 * Surfer dark theme, loaded directly from the gresource (deterministic, no
 * dependency on the system icon theme). See the gresource manifest. */
#define GW_PHOSPHOR_ICON_SIZE 16

static GdkPixbuf *load_hier_icon(const char *name)
{
    char *path =
        g_strdup_printf("/io/github/gtkwave/GTKWave/icons/phosphor/%s.svg", name);
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_resource_at_scale(path,
                                                              GW_PHOSPHOR_ICON_SIZE,
                                                              GW_PHOSPHOR_ICON_SIZE,
                                                              TRUE,
                                                              &error);
    if (pixbuf == NULL) {
        g_warning("could not load hierarchy icon '%s': %s",
                  path,
                  error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
    }
    g_free(path);
    return pixbuf;
}

GwHierarchyIcons *gw_hierarchy_icons_new(void)
{
    GwHierarchyIcons *self = g_new0(GwHierarchyIcons, 1);

    /* Verilog */
    self->module = load_hier_icon("hier-module");
    self->task = load_hier_icon("hier-task");
    self->function = load_hier_icon("hier-function");
    self->begin = load_hier_icon("hier-begin");
    self->fork = load_hier_icon("hier-fork");

    /* SV */
    self->interface = load_hier_icon("hier-interface");
    self->svpackage = load_hier_icon("hier-svpackage");
    self->program = load_hier_icon("hier-program");
    self->class = load_hier_icon("hier-class");

    /* VHDL */
    self->design = load_hier_icon("hier-design");
    self->block = load_hier_icon("hier-block");
    self->generateif = load_hier_icon("hier-generateif");
    self->generatefor = load_hier_icon("hier-generatefor");
    self->instance = load_hier_icon("hier-instance");
    self->package = load_hier_icon("hier-package");

    self->signal = load_hier_icon("hier-signal");
    self->portin = load_hier_icon("hier-portin");
    self->portout = load_hier_icon("hier-portout");
    self->portinout = load_hier_icon("hier-portinout");
    self->buffer = load_hier_icon("hier-buffer");
    self->linkage = load_hier_icon("hier-linkage");

    /* FSDB VHDL (on top of GHW's existing) */
    self->record = load_hier_icon("hier-record");
    self->generate = load_hier_icon("hier-generate");

    return self;
}

GdkPixbuf *gw_hierarchy_icons_get(GwHierarchyIcons *self, guint tree_kind)
{
    g_return_val_if_fail(self != NULL, NULL);

    // clang-format off
    switch (tree_kind) {
        case GW_TREE_KIND_VCD_ST_MODULE:    return self->module;
        case GW_TREE_KIND_VCD_ST_TASK:      return self->task;
        case GW_TREE_KIND_VCD_ST_FUNCTION:  return self->function;
        case GW_TREE_KIND_VCD_ST_BEGIN:     return self->begin;
        case GW_TREE_KIND_VCD_ST_FORK:      return self->fork;
        case GW_TREE_KIND_VCD_ST_GENERATE:  return self->generatefor; // same as GW_TREE_KIND_VHDL_ST_GENFOR
        case GW_TREE_KIND_VCD_ST_STRUCT:    return self->block;       // same as GW_TREE_KIND_VHDL_ST_BLOCK
        case GW_TREE_KIND_VCD_ST_UNION:     return self->instance;    // same as GW_TREE_KIND_VHDL_ST_INSTANCE
        case GW_TREE_KIND_VCD_ST_CLASS:     return self->class;
        case GW_TREE_KIND_VCD_ST_INTERFACE: return self->interface;
        case GW_TREE_KIND_VCD_ST_PACKAGE:   return self->svpackage;
        case GW_TREE_KIND_VCD_ST_PROGRAM:   return self->program;

        case GW_TREE_KIND_VHDL_ST_DESIGN:   return self->design;
        case GW_TREE_KIND_VHDL_ST_BLOCK:    return self->block;
        case GW_TREE_KIND_VHDL_ST_GENIF:    return self->generateif;
        case GW_TREE_KIND_VHDL_ST_GENFOR:   return self->generatefor;
        case GW_TREE_KIND_VHDL_ST_INSTANCE: return self->instance;
        case GW_TREE_KIND_VHDL_ST_PACKAGE:  return self->package;

        case GW_TREE_KIND_VHDL_ST_SIGNAL:    return self->signal;
        case GW_TREE_KIND_VHDL_ST_PORTIN:    return self->portin;
        case GW_TREE_KIND_VHDL_ST_PORTOUT:   return self->portout;
        case GW_TREE_KIND_VHDL_ST_PORTINOUT: return self->portinout;
        case GW_TREE_KIND_VHDL_ST_BUFFER:    return self->buffer;
        case GW_TREE_KIND_VHDL_ST_LINKAGE:   return self->linkage;

        case GW_TREE_KIND_VHDL_ST_ARCHITECTURE: return self->module; // same as GW_TREE_KIND_VCD_ST_MODULE
        case GW_TREE_KIND_VHDL_ST_FUNCTION:     return self->function; // same as GW_TREE_KIND_VCD_ST_FUNCTION
        case GW_TREE_KIND_VHDL_ST_PROCESS:      return self->task; // same as GW_TREE_KIND_VCD_ST_TASK
        case GW_TREE_KIND_VHDL_ST_PROCEDURE:    return self->class; // same as GW_TREE_KIND_VCD_ST_CLASS
        case GW_TREE_KIND_VHDL_ST_RECORD:       return self->record;
        case GW_TREE_KIND_VHDL_ST_GENERATE:     return self->generate;

        default:
            return NULL;
    }
    // clang-format on
}
