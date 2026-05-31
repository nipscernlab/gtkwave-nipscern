#!/usr/bin/env bash
# ============================================================
#  GTKWave (nipscern) - build interno (MSYS2 / MINGW64)
#  Chamado pelo build-windows.bat. Nao rode direto do cmd;
#  precisa estar num shell MINGW64 (MSYSTEM=MINGW64).
#
#  Argumento opcional: "clean" -> apaga build/ antes.
# ============================================================
set -euo pipefail

# Posiciona na raiz do projeto (tools/..), robusto ao diretorio de onde
# o script foi chamado. Assim 'meson setup build' cria build/ na raiz.
cd "$(dirname "${BASH_SOURCE[0]}")/.." || {
    echo "[msys2-build] ERRO: nao consegui achar a raiz do projeto" >&2
    exit 1
}

# Prefixo de instalacao (caminho estilo Windows e' aceito pelo meson)
PREFIX="${GTKWAVE_PREFIX:-C:/packs/gtkwave-bin}"

# Garante que estamos no MINGW64 (gtk3 etc. vem desse subsistema)
if [ "${MSYSTEM:-}" != "MINGW64" ]; then
    echo "[msys2-build] AVISO: MSYSTEM='${MSYSTEM:-}' (esperado MINGW64)." >&2
fi

# ------------------------------------------------------------
# 1) Toolchain: instala se faltar QUALQUER ferramenta essencial.
#    Checa meson/gcc/git de forma independente (git e' preciso
#    para clonar o subprojeto libfst via meson wrap-git).
# ------------------------------------------------------------
need_install=0
for tool in meson gcc git; do
    command -v "$tool" >/dev/null 2>&1 || need_install=1
done
if [ "$need_install" = "1" ]; then
    echo "[msys2-build] Instalando toolchain GTK3/GTK4 + meson + git (pode demorar)..."
    pacman -Sy --noconfirm
    pacman -S --needed --noconfirm \
        mingw-w64-x86_64-toolchain \
        mingw-w64-x86_64-gtk3 \
        mingw-w64-x86_64-gtk4 \
        mingw-w64-x86_64-pkgconf \
        mingw-w64-x86_64-glib2 \
        mingw-w64-x86_64-gobject-introspection \
        mingw-w64-x86_64-gperf \
        mingw-w64-x86_64-meson \
        mingw-w64-x86_64-ninja \
        mingw-w64-x86_64-desktop-file-utils \
        git \
        flex
fi

# ------------------------------------------------------------
# 2) Configurar / compilar / instalar
# ------------------------------------------------------------
if [ "${1:-}" = "clean" ]; then
    echo "[msys2-build] Limpando build/ ..."
    rm -rf build
fi

# build/ so' e' valido se o meson setup concluiu (gera build.ninja).
# Um setup que falhou (ex.: faltava git p/ clonar o libfst) deixa a
# pasta build/ pela metade -> detecta e refaz do zero.
if [ ! -f build/build.ninja ]; then
    if [ -d build ]; then
        echo "[msys2-build] build/ incompleto -> refazendo do zero..."
        rm -rf build
    fi
    echo "[msys2-build] meson setup (prefix=$PREFIX) ..."
    # tests=false: evita a dependencia 'diff' e acelera rebuilds (a suite
    # de testes nao e' necessaria para gerar/usar o gtkwave.exe).
    meson setup --prefix="$PREFIX" -Djudy=disabled -Dtests=false build
fi

echo "[msys2-build] meson compile + install ..."
meson install -C build

# ------------------------------------------------------------
# 3) Gerar o launcher gtkwave.cmd (ajusta o PATH das DLLs do GTK)
# ------------------------------------------------------------
MINGW_BIN_WIN="$(cygpath -w /mingw64/bin)"
PREFIX_DIR="$(cygpath -u "$PREFIX")"
LAUNCHER="$PREFIX_DIR/gtkwave.cmd"

# Heredoc nao-aspas: $MINGW_BIN_WIN expande; %PATH%, %~dp0, %* ficam literais.
cat > "$LAUNCHER" <<EOF
@echo off
rem Launcher gerado por tools/msys2-build.sh - ajusta o PATH das DLLs do GTK.
rem --dark: abre sempre no tema escuro (nipscern).
set "PATH=$MINGW_BIN_WIN;%PATH%"
"%~dp0bin\gtkwave.exe" --dark %*
EOF

# ------------------------------------------------------------
# 4) Deploy para o pacote standalone do projeto AURORA
#    (nao-fatal: se falhar, o build/instalacao local continua OK)
# ------------------------------------------------------------
script_dir="$(dirname "${BASH_SOURCE[0]}")"
if ! bash "$script_dir/deploy-aurora.sh"; then
    echo "[msys2-build] aviso: deploy para o AURORA falhou (build local OK)." >&2
fi

echo "[msys2-build] OK"
echo "[msys2-build]   exe:      $PREFIX/bin/gtkwave.exe"
echo "[msys2-build]   launcher: $(cygpath -w "$LAUNCHER")"
