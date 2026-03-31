seq 35 45 | while read -r x; do
  seq -w 1 10 | while read -r y; do
    printf '%s\n' "https://srtm.csi.cgiar.org/wp-content/uploads/files/srtm_5x5/TIFF/srtm_${x}_${y}.zip"
  done
done | xargs -n 1 -P 8 wget -nc

find . -maxdepth 1 -name 'srtm_*.zip' -print0 | xargs -0 -n 1 -P 8 unzip -o

then run script `build_srtm_italy.sh`