#!/bin/sh
# Stub for qml toolchain programs (qmltyperegistrar, etc.)
# Creates stubbed output files when --generate-qmltypes or -o is seen.
# For .cpp output, generates a minimal valid registration function stub
# to satisfy linker references (qml_register_types_*).
# For .qmltypes output, generates an empty QML types XML file.
# Handles multiple output files in a single invocation.
set -e

output_file=""
qmltypes_file=""
jsroot_file=""
import_name=""
major_version=1
minor_version=0
namespace=""

while [ $# -gt 0 ]; do
    case "$1" in
        -o=*|--output=*)
            output_file="${1#*=}"
            ;;
        -o|--output)
            shift
            output_file="$1"
            ;;
        --generate-qmltypes=*)
            qmltypes_file="${1#*=}"
            ;;
        --generate-qmltypes)
            shift
            qmltypes_file="$1"
            ;;
        --jsroot)
            # flag only, handled by --generate-qmltypes for qmljsrootgen
            ;;
        --import-name=*)
            import_name="${1#*=}"
            ;;
        --import-name)
            shift
            import_name="$1"
            ;;
        --major-version=*)
            major_version="${1#*=}"
            ;;
        --major-version)
            shift
            major_version="$1"
            ;;
        --minor-version=*)
            minor_version="${1#*=}"
            ;;
        --minor-version)
            shift
            minor_version="$1"
            ;;
        --namespace=*)
            namespace="${1#*=}"
            ;;
        --namespace)
            shift
            namespace="$1"
            ;;
        --past-major-version|--follow-foreign-versioning|--private-includes)
            # flags with no value or handled values -- skip
            ;;
        @*)
            # foreign types file reference -- skip
            ;;
        *.json)
            # positional arg (metatypes JSON) -- skip
            ;;
        *)
            # skip unknown args
            ;;
    esac
    shift
done

# Sanitize import name to create symbol name: replace non-alphanumeric with _
module_symbol=$(echo "$import_name" | sed 's/[^a-zA-Z0-9]/_/g')

# Create output cpp file with registration function stub
if [ -n "$output_file" ]; then
    mkdir -p "$(dirname "$output_file")"
    case "$output_file" in
        *.cpp)
            if [ -n "$import_name" ]; then
                cat > "$output_file" << CPPEOF
#include <QtQml/qqml.h>
#include <QtQml/qqmlprivate.h>
#include <QtQml/qqmlmoduleregistration.h>

#if !defined(QT_STATIC)
#define Q_QMLTYPE_EXPORT Q_DECL_EXPORT
#else
#define Q_QMLTYPE_EXPORT
#endif

QT_BEGIN_NAMESPACE

Q_QMLTYPE_EXPORT void qml_register_types_${module_symbol}()
{
    qmlRegisterModule("${import_name}", ${major_version}, ${minor_version});
}

QT_END_NAMESPACE
CPPEOF
            else
                cat > "$output_file" << 'CPPEOF'
#include <QtQml/qqml.h>
#include <QtQml/qqmlprivate.h>
QT_BEGIN_NAMESPACE
QT_END_NAMESPACE
CPPEOF
            fi
            ;;
        *)
            touch "$output_file"
            ;;
    esac
fi

# Create qmltypes file
if [ -n "$qmltypes_file" ]; then
    mkdir -p "$(dirname "$qmltypes_file")"
    cat > "$qmltypes_file" << 'QMLTYPEOF'
<?xml version="1.0" encoding="UTF-8"?>
<QMLTypes>
</QMLTypes>
QMLTYPEOF
fi

# Create jsroot file
if [ -n "$jsroot_file" ]; then
    mkdir -p "$(dirname "$jsroot_file")"
    cat > "$jsroot_file" << 'QMLTYPEOF'
<?xml version="1.0" encoding="UTF-8"?>
<QMLTypes>
</QMLTypes>
QMLTYPEOF
fi

exit 0
