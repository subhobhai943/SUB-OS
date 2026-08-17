#!/usr/bin/env bash
# ==============================================================================
# SUB-OS Global Remote Access & Internet Tunnel Launcher
# Enables SSH & Web access from ANY network (different Wi-Fi, 4G/5G, Internet)
# ==============================================================================

set -e

echo "================================================================="
echo "   SUB-OS Global Internet Remote Access & Tunnel Manager         "
echo "================================================================="
echo ""
echo "Select how you want to expose SUB-OS SSH to the global Internet:"
echo ""
echo "  [1] Pinggy Instant SSH Tunnel (Zero setup, works anywhere worldwide)"
echo "  [2] Serveo SSH Reverse Proxy (Zero setup, global public domain)"
echo "  [3] Cloudflare Zero-Trust Tunnel (cloudflared)"
echo "  [4] Ngrok TCP Forwarder"
echo "  [5] Local Subnet Only (Same Wi-Fi / LAN IP)"
echo ""
read -p "Choose option [1-5] (default: 1): " OPTION
OPTION=${OPTION:-1}

case "$OPTION" in
    1)
        echo ""
        echo "🚀 Starting Pinggy Public Tunnel for SUB-OS SSH (Port 2222)..."
        echo "👉 Once connected, use the public command shown below on your Kali terminal!"
        echo ""
        ssh -p 443 -R0:localhost:2222 -o StrictHostKeyChecking=no a.pinggy.io
        ;;
    2)
        echo ""
        echo "🚀 Starting Serveo Reverse Tunnel for SUB-OS SSH (Port 2222)..."
        echo "👉 Once connected, use the command displayed on screen from any network!"
        echo ""
        ssh -R 0:localhost:2222 serveo.net
        ;;
    3)
        echo ""
        echo "🚀 Launching Cloudflare Tunnel for SUB-OS..."
        if ! command -v cloudflared &> /dev/null; then
            echo "Error: cloudflared is not installed. Install via: 'brew install cloudflared' or 'apt install cloudflared'"
            exit 1
        fi
        cloudflared tunnel --url tcp://localhost:2222
        ;;
    4)
        echo ""
        echo "🚀 Launching Ngrok TCP Tunnel for Port 2222..."
        if ! command -v ngrok &> /dev/null; then
            echo "Error: ngrok is not installed. Download from https://ngrok.com"
            exit 1
        fi
        ngrok tcp 2222
        ;;
    5)
        HOST_IP=$(hostname -I 2>/dev/null | awk '{print $1}' || ipconfig getifaddr en0 2>/dev/null || echo "127.0.0.1")
        echo ""
        echo "📍 Local Wi-Fi / LAN Mode:"
        echo "👉 On Kali Linux (same Wi-Fi), run:"
        echo "   ssh -p 2222 root@$HOST_IP"
        echo "   curl http://$HOST_IP:8080/api/status"
        ;;
    *)
        echo "Invalid option. Exiting."
        exit 1
        ;;
esac
