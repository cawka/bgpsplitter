git log > ChangeLog
git log | sed -n -e 's/^Author: *//p' | uniq > AUTHORS
autoreconf --install --force

