from pystac_client import Client
import planetary_computer
import requests
import os

catalog = Client.open("https://planetarycomputer.microsoft.com/api/stac/v1")

# Nairobi-centered region; adjust bbox if you want a tighter area
search = catalog.search(
    collections=["sentinel-2-l2a"],
    bbox=[36.6, -1.5, 37.1, -1.0],
    datetime="2024-01-01/2024-12-31",
    query={"eo:cloud_cover": {"lt": 5}},
)

items = sorted(list(search.get_items()), key=lambda x: x.properties["eo:cloud_cover"])[:10]

bands = ["B02", "B03", "B04", "B08"]

os.makedirs("raw", exist_ok=True)

for item in items:
    signed = planetary_computer.sign(item)
    print(item.id, item.properties.get("eo:cloud_cover"))

    for band in bands:
        url = signed.assets[band].href
        out = os.path.join("raw", f"{item.id}_{band}.tif")
        if os.path.exists(out):
            continue
        r = requests.get(url, stream=True)
        r.raise_for_status()
        with open(out, "wb") as f:
            for chunk in r.iter_content(1024 * 1024):
                if chunk:
                    f.write(chunk)