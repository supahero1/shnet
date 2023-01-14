#!/bin/bash
echo "#!/usr/bin/sed -f" > .sed.temp
echo "s/%${PROJECT_NAME_UP}_VERSION%/$VERSION/" >> .sed.temp
echo "s/%${PROJECT_NAME_UP}_PATCH%/$PATCH/" >> .sed.temp
echo "s/%${PROJECT_NAME_UP}_CLI_VERSION%/$CLI_VERSION/" >> .sed.temp
echo "s/%${PROJECT_NAME_UP}_CLI_PATCH%/$CLI_PATCH/" >> .sed.temp
chmod u+x .sed.temp
result=$(find . -name "in.*" -not -path "./.git*")
for file in $result
do
  my_dirname=$(dirname $file)/
  my_basename=$(basename $file)
  my_filename=${my_basename:3}
  if test -f "${my_dirname}${my_filename}"; then
    ./.sed.temp $file > "${my_dirname}.${my_filename}.temp"
    old_checksum=$(md5sum $my_dirname$my_filename)
    old_checksum="${old_checksum:0:32}"
    new_checksum=$(md5sum $my_dirname.$my_filename.temp)
    new_checksum="${old_checksum:0:32}"
    if [ "${old_checksum}" != "${new_checksum}" ]; then
      mv -f $my_dirname.$my_filename.temp $my_dirname$my_filename
    fi
    rm -f $my_dirname.$my_filename.temp
  else
    ./.sed.temp $file > "${my_dirname}${my_filename}"
  fi
done
rm -f .sed.temp
