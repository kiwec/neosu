#!/bin/bash

if [ -z "$1" ]; then
  echo "Usage: $0 <new-version>"
  exit 1
fi

VERSION="$1"                 # 39.03
VERSION_RC="${VERSION//./,}" # 39,03
VERSION_CL="${VERSION//./_}" # 39_03

sed -Ei "s/version=\"[0-9]+\.[0-9]+\.0\.0\"/version=\"$VERSION.0.0\"/" assets/neosu.manifest

sed -Ei "s/[0-9]+,[0-9]+,0,0/$VERSION_RC,0,0/g" assets/resource.rc
sed -Ei "s/(\"FileVersion\", \")[0-9]+\.[0-9]+\.0\.0/\1$VERSION.0.0/" assets/resource.rc
sed -Ei "s/(\"ProductVersion\", \")[0-9]+\.[0-9]+/\1$VERSION/" assets/resource.rc

sed -Ei "2s/[0-9]+\.[0-9]+/$VERSION/" cmake-win/src/CMakeLists.txt

sed -Ei "2s/[0-9]+\.[0-9]+/$VERSION/" configure.ac
autoconf

sed -i "/std::vector<CHANGELOG> changelogs;/a\\
\\
    CHANGELOG v$VERSION_CL;\\
    v$VERSION_CL.title = \"$VERSION\";\\
    v$VERSION_CL.changes = {\\
        R\"()\",\\
    };\\
    changelogs.push_back(v$VERSION_CL);" src/App/Osu/Changelog.cpp
