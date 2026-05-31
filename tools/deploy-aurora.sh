#!/usr/bin/env bash
# ============================================================
#  nipscern: deploy do gtkwave recem-compilado para o pacote
#  standalone do projeto AURORA.
#
#  A pasta do AURORA e' um bundle GTK portatil (exe + todas as
#  DLLs + loaders + schemas na propria pasta), com layout
#  diferente do install do meson (que usa bin/ e depende do PATH
#  do msys64). Este script reconstroi esse bundle a partir do
#  build atual + do mingw64 contra o qual ele foi linkado,
#  garantindo versoes consistentes.
#
#  Chamado no fim de tools/msys2-build.sh. Precisa rodar dentro
#  de um shell MINGW64. Variaveis:
#    GTKWAVE_PREFIX      install do meson (default C:/packs/gtkwave-bin)
#    GTKWAVE_AURORA_DIR  destino (default: pasta do AURORA abaixo)
#    GTKWAVE_NO_DEPLOY=1 pula o deploy
# ============================================================
set -euo pipefail

PREFIX="${GTKWAVE_PREFIX:-C:/packs/gtkwave-bin}"
AURORA_WIN="${GTKWAVE_AURORA_DIR:-C:/Users/chrys/Documents/GitHub/aurora/components/Packages/gtkwave-nipscern}"

if [ "${GTKWAVE_NO_DEPLOY:-0}" = "1" ]; then
    echo "[deploy-aurora] desativado (GTKWAVE_NO_DEPLOY=1)."
    exit 0
fi

prefix_u="$(cygpath -u "$PREFIX")"
aurora_u="$(cygpath -u "$AURORA_WIN")"

# Se o projeto AURORA nao existe nesta maquina, apenas pula.
if [ ! -d "$(dirname "$aurora_u")" ]; then
    echo "[deploy-aurora] AURORA nao encontrado ($(dirname "$aurora_u")) - pulando."
    exit 0
fi
if [ ! -x "$prefix_u/bin/gtkwave.exe" ]; then
    echo "[deploy-aurora] ERRO: $PREFIX/bin/gtkwave.exe nao existe (rode o build antes)." >&2
    exit 1
fi

echo "[deploy-aurora] destino: $AURORA_WIN"
mkdir -p "$aurora_u"

# ------------------------------------------------------------
# 1) Executaveis (gtkwave.exe + utilitarios) na raiz do bundle
# ------------------------------------------------------------
echo "[deploy-aurora] (1/6) executaveis"
cp -f "$prefix_u"/bin/*.exe "$aurora_u"/

# ------------------------------------------------------------
# 2) DLLs: limpa as antigas, repoe as nossas + todo o runtime
#    do GTK (deps mingw64 resolvidas via ldd dos exes E dos
#    loaders do gdk-pixbuf - estes puxam o librsvg, essencial
#    para os icones SVG).
# ------------------------------------------------------------
echo "[deploy-aurora] (2/6) DLLs (nossas + runtime GTK via ldd)"
rm -f "$aurora_u"/*.dll
find "$prefix_u"/bin -maxdepth 1 -name '*.dll' -exec cp -f {} "$aurora_u"/ \;

ldd_targets=("$prefix_u"/bin/*.exe)
for ld in /mingw64/lib/gdk-pixbuf-2.0/2.10.0/loaders/*.dll; do
    [ -f "$ld" ] && ldd_targets+=("$ld")
done

deps="$(for t in "${ldd_targets[@]}"; do ldd "$t" 2>/dev/null; done \
        | awk '/\/mingw64\/bin\// {print $3}' | sort -u)"
printf '%s\n' "$deps" | while read -r dll; do
    if [ -n "$dll" ] && [ -f "$dll" ]; then
        cp -f "$dll" "$aurora_u"/
    fi
done

# ------------------------------------------------------------
# 3) Dados do gtkwave + icone do app
# ------------------------------------------------------------
echo "[deploy-aurora] (3/6) share/gtkwave3 + icones do app"
mkdir -p "$aurora_u"/share
rm -rf "$aurora_u"/share/gtkwave3
cp -r "$prefix_u"/share/gtkwave3 "$aurora_u"/share/
if [ -d "$prefix_u/share/icons/hicolor" ]; then
    mkdir -p "$aurora_u"/share/icons
    cp -r "$prefix_u"/share/icons/hicolor "$aurora_u"/share/icons/
fi

# ------------------------------------------------------------
# 4) Loaders do gdk-pixbuf (renderizam os SVG dos icones)
# ------------------------------------------------------------
echo "[deploy-aurora] (4/6) gdk-pixbuf loaders"
loaders_rel="lib/gdk-pixbuf-2.0/2.10.0/loaders"
rm -rf "$aurora_u/lib/gdk-pixbuf-2.0"
mkdir -p "$aurora_u/$loaders_rel"
cp -f /mingw64/lib/gdk-pixbuf-2.0/2.10.0/loaders/*.dll "$aurora_u/$loaders_rel/" 2>/dev/null || true

# ------------------------------------------------------------
# 5) loaders.cache PORTATIL (caminhos relativos a raiz do bundle)
# ------------------------------------------------------------
echo "[deploy-aurora] (5/6) loaders.cache portatil"
(
    cd "$aurora_u"
    GDK_PIXBUF_MODULEDIR="$loaders_rel" \
        /mingw64/bin/gdk-pixbuf-query-loaders.exe \
        > "lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"
)

# ------------------------------------------------------------
# 6) glib schemas + tema de icones Adwaita (fallback do GTK)
# ------------------------------------------------------------
echo "[deploy-aurora] (6/6) glib schemas + Adwaita"
mkdir -p "$aurora_u"/share/glib-2.0/schemas
cp -f /mingw64/share/glib-2.0/schemas/gschemas.compiled \
      "$aurora_u"/share/glib-2.0/schemas/ 2>/dev/null || true
if [ ! -d "$aurora_u/share/icons/Adwaita" ]; then
    mkdir -p "$aurora_u"/share/icons
    cp -r /mingw64/share/icons/Adwaita "$aurora_u"/share/icons/ 2>/dev/null || true
fi

dll_count="$(find "$aurora_u" -maxdepth 1 -name '*.dll' | wc -l)"
echo "[deploy-aurora] OK -> $AURORA_WIN  ($dll_count DLLs na raiz)"
