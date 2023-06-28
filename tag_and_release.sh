# This script is used to tag a new release version of the project.

# this script can be used on windows with git bash
# or on linux

# when to increment which version number?
# major: when you make incompatible API changes
# minor: when you add functionality in a backwards-compatible manner
# patch: when you make backwards-compatible bug fixes


# input parameters:
# major=(true/false), minor=(true/false), patch=(true/false): to increase the version number to be released
# extra=(string): to add an extra string to the version number to be released (e.g. "beta1", "test", "pre-release", etc.)
MAJOR=$1 
MINOR=$2
PATCH=$3
EXTRA=$4

# get current version number from src/resource/version.h 
# this is the format used in the file:
#define VERSION_MAJOR               3
#define VERSION_MINOR               5
#define VERSION_REVISION            0
#define VERSION_BUILD               214

MAJOR_VERSION=$(grep -oP '(?<=VERSION_MAJOR               )[^ ]*' src/resource/version.h)
MINOR_VERSION=$(grep -oP '(?<=VERSION_MINOR               )[^ ]*' src/resource/version.h)
PATCH_VERSION=$(grep -oP '(?<=VERSION_REVISION            )[^ ]*' src/resource/version.h)

echo MAJOR_VERSION: $MAJOR_VERSION
echo MINOR_VERSION: $MINOR_VERSION
echo PATCH_VERSION: $PATCH_VERSION

# if a version number change is requested
if [ "$MAJOR" = true ] || [ "$MINOR" = true ] || [ "$PATCH" = true ] ; then
    # check if the version number is valid
    if [ "$MAJOR" = true ] && [ "$MINOR" = true ] ; then
        echo "ERROR: major and minor version number cannot be increased at the same time"
        exit 1
    fi
    if [ "$MAJOR" = true ] && [ "$PATCH" = true ] ; then
        echo "ERROR: major and patch version number cannot be increased at the same time"
        exit 1
    fi
    if [ "$MINOR" = true ] && [ "$PATCH" = true ] ; then
        echo "ERROR: minor and patch version number cannot be increased at the same time"
        exit 1
    fi
    if [ "$MAJOR" = true ] && [ "$MINOR" = true ] && [ "$PATCH" = true ] ; then
        echo "ERROR: major, minor and patch version number cannot be increased at the same time"
        exit 1
    fi

    # increase version number
    if [ "$MAJOR" = true ] ; then
        MAJOR_VERSION=$((MAJOR_VERSION+1))
        MINOR_VERSION=0
        PATCH_VERSION=0
    elif [ "$MINOR" = true ] ; then
        MINOR_VERSION=$((MINOR_VERSION+1))
        PATCH_VERSION=0
    elif [ "$PATCH" = true ] ; then
        PATCH_VERSION=$((PATCH_VERSION+1))
    fi

    # insert increased version number in src/resource/version.h
    sed -i "s/VERSION_MAJOR               .*/VERSION_MAJOR               ${MAJOR_VERSION}/" src/resource/version.h
    sed -i "s/VERSION_MINOR               .*/VERSION_MINOR               ${MINOR_VERSION}/" src/resource/version.h
    sed -i "s/VERSION_REVISION            .*/VERSION_REVISION            ${PATCH_VERSION}/" src/resource/version.h
    # change file to dos format, since git bash on windows has convert crlf to lf
    unix2dos src/resource/version.h

    # commit version file
    git add src/resource/version.h
    git commit -a -m "version ${NEW_VERSION}"
    git push
fi

# create new version number
NEW_VERSION="${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"

# add extra string to version number
if [ ! -z "$EXTRA" ] ; then
    NEW_VERSION="${NEW_VERSION}.${EXTRA}"
fi

# create a git tag with the version number
echo "TAG: v${NEW_VERSION}"
git tag -a v${NEW_VERSION} -m "version ${NEW_VERSION}"
# push the tag to the remote repository
git push origin v${NEW_VERSION}
