import asyncio
import json
from bleak import BleakClient

MAC = "98:88:E0:10:1F:2E"
CHAR_UUID = "12345678-1234-5678-1234-56789abcdef1"

def notification_handler(sender, data):
    """Affiche les données des capteurs de façon lisible"""
    try:
        # Décoder le JSON
        text = data.decode('utf-8')
        sensor_data = json.loads(text)
        
        # Affichage formaté
        print("\n" + "="*50)
        print("📊 SENSOR DATA")
        print("="*50)
        print(f"🌡️  Temperature:  {sensor_data['temp']:.1f} °C")
        print(f"📐 Accelerometer:")
        print(f"     X: {sensor_data['ax']:+7.2f} m/s²")
        print(f"     Y: {sensor_data['ay']:+7.2f} m/s²")
        print(f"     Z: {sensor_data['az']:+7.2f} m/s²")
        print(f"🔋 Battery:       {sensor_data['batt']:.2f} V")
        print("="*50)
        
    except json.JSONDecodeError:
        print(f"⚠️  Invalid JSON: {text}")
    except KeyError as e:
        print(f"⚠️  Missing key: {e}")
        print(f"📦 Raw data: {text}")
    except Exception as e:
        print(f"❌ Error: {e}")
        print(f"📦 Raw HEX: {data.hex()}")

async def main():
    print(f"🔍 Connecting to {MAC}...")
    
    async with BleakClient(MAC, timeout=15.0) as client:
        print(f"✅ Connected to ESP32!")
        print(f"🔔 Activating notifications...")
        
        await client.start_notify(CHAR_UUID, notification_handler)
        
        print(f"✅ Notifications enabled!")
        print(f"📡 Receiving sensor data... (press Ctrl+C to stop)\n")
        
        try:
            # Recevoir indéfiniment
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\n\n👋 Stopping...")
            await client.stop_notify(CHAR_UUID)
            print("✅ Disconnected")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 Bye!")
