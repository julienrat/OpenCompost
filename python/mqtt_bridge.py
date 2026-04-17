
import subprocess
import json
import time
import paho.mqtt.client as mqtt
import os

# =========================
# CONFIG
# =========================
NODE_ID = "A6FBE54E"

SERIAL_PORT = "/dev/ttyACM1"

BROKER = "mqtt.gaulix.fr"
PORT = 1883
CLIENT_ID = "julienrat"



# Identifiants (via variables d'environnement recommandé)
USERNAME = os.getenv("MQTT_USER", "julienrat")
PASSWORD = os.getenv("MQTT_PASS", "u6QdWuuLzO!%Pjn!ekSh8wbi")

TOPIC_OUT = "meshcore/compagnon/Sensor/Compost"

INTERVAL = 60  # secondes

# =========================
# MQTT INIT
# =========================
client = mqtt.Client(client_id=CLIENT_ID)
client.username_pw_set(USERNAME, PASSWORD)
client.connect(BROKER, PORT, 60)
client.loop_start()

# =========================
# TELEMETRIE
# =========================
def get_telemetry():
    cmd = [
        "meshcli",
        "-s", SERIAL_PORT,
        "-j",
        ".req_telemetry",
        NODE_ID
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)

    try:
        return json.loads(result.stdout)
    except Exception as e:
        print("[ERROR PARSE]", e)
        print("[RAW OUTPUT]", result.stdout)
        return None

# =========================
# EXTRACTION LPP
# =========================
def parse_lpp(data):
    voltage = None
    temperature = None

    lpp = data.get("lpp", [])

    for item in lpp:
        if item.get("type") == "voltage":
            voltage = item.get("value")

        elif item.get("type") == "temperature":
            temperature = item.get("value")

    return voltage, temperature

# =========================
# LOOP
# =========================
print("[START] MeshCore → MQTT compost")

while True:
    data = get_telemetry()

    if data:
        voltage, temperature = parse_lpp(data)

        payload = {
            "node": NODE_ID,
            "voltage": voltage,
            "temperature": temperature,
            "raw": data
        }

        print("\n[TELEMETRIE]")
        print(payload)

        client.publish(TOPIC_OUT, json.dumps(payload))

    else:
        print("[WARN] aucune donnée")

    time.sleep(INTERVAL)
