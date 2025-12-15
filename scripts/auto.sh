#!/bin/sh

# =============================================================================
#  Bionic Cat Combined Auto Startup Script
#  Merges functionality of:
#  1. System Infrastructure (WiFi, SSH, FRP)
#  2. Bionic Cat Application Modules (Env vars, Dependency checks, App start)
# =============================================================================

# --- 1. Global Configuration & Paths ---
ROOT_DIR="/mnt/data"
BASE_DIR="${ROOT_DIR}/CV184X/bionic_cat"
LOG_DIR="${BASE_DIR}/logs"

# Ensure log directory exists
mkdir -p "${LOG_DIR}"

export WIFI_SSID="FinnoxTest"
export WIFI_PASS="12345678"

# --- 2. Environment Variables (Critical) ---
# Source general environment
if [ -f "${ROOT_DIR}/env_setup.sh" ]; then
    source "${ROOT_DIR}/env_setup.sh"
fi

# Explicitly export the complex Library Paths from start_all_modules.sh
# This overrides/appends to ensure specific 3rd party libs are found.
export LD_LIBRARY_PATH="${BASE_DIR}/3rd/lvgl/lib:${BASE_DIR}/3rd/cmsis_dsp/lib:${BASE_DIR}/3rd/cvi_mpi/lib:${BASE_DIR}/3rd/fdk_aac/lib:${BASE_DIR}/3rd/glog/lib:${BASE_DIR}/3rd/ini/lib:${BASE_DIR}/3rd/mosquitto/lib:${BASE_DIR}/3rd/paho_mqtt_c/lib:${BASE_DIR}/3rd/paho_mqtt_cpp/lib:${BASE_DIR}/3rd/tinyalsa/lib:${BASE_DIR}/3rd/yaml_cpp/lib:${ROOT_DIR}/CV184X/lib:${ROOT_DIR}/CV184X/sample/3rd/opencv/lib:${ROOT_DIR}/CV184X/sample/utils/lib:/lib:/lib/3rd:/lib/arm-linux-gnueabihf:/usr/lib:/usr/local/lib:/system/lib:/system/usr/lib:/system/usr/lib/3rd:${ROOT_DIR}/lib"

export LIBRARY_PATH="${BASE_DIR}/3rd/lvgl/lib:${BASE_DIR}/3rd/cmsis_dsp/lib:${BASE_DIR}/3rd/cvi_mpi/lib:${BASE_DIR}/3rd/fdk_aac/lib:${BASE_DIR}/3rd/glog/lib:${BASE_DIR}/3rd/ini/lib:${BASE_DIR}/3rd/mosquitto/lib:${BASE_DIR}/3rd/paho_mqtt_c/lib:${BASE_DIR}/3rd/paho_mqtt_cpp/lib:${BASE_DIR}/3rd/tinyalsa/lib:${BASE_DIR}/3rd/yaml_cpp/lib"

export SSL_CERT_FILE="${ROOT_DIR}/ssl/cacert.pem"

# --- 3. Helper Function to Start Applications ---
start_app() {
    BIN_PATH="$1"
    APP_NAME="$2"
    LOG_FILE="${LOG_DIR}/${APP_NAME}.log"

    if [ ! -f "$BIN_PATH" ]; then
        echo "[ERROR] Executable not found: $BIN_PATH"
        return 1
    fi

    if [ ! -x "$BIN_PATH" ]; then
        echo "[WARN] Making $APP_NAME executable..."
        chmod +x "$BIN_PATH"
    fi

    echo "[INFO] Starting $APP_NAME..."
    # Run in background with nohup
    nohup "$BIN_PATH" > "$LOG_FILE" 2>&1 &
    PID=$!
    echo "[INFO] $APP_NAME started (PID: $PID)"
    sleep 1
}

# --- 4. System Infrastructure Startup ---

echo "[INFO] Connect to wifi..."
cd "${ROOT_DIR}" || echo "[WARN] Could not cd to ${ROOT_DIR}"
if [ -f "./wifi.sh" ]; then
    ./wifi.sh > "${ROOT_DIR}/wifi.log" 2>&1 &
else
    echo "[WARN] wifi.sh not found in ${ROOT_DIR}"
fi

echo "[INFO] Starting FRP..."
if [ -f "${ROOT_DIR}/frp/frpc" ]; then
    "${ROOT_DIR}/frp/frpc" -c "${ROOT_DIR}/frp/frpc.ini" > /dev/null 2>&1 &
fi

echo "[INFO] Starting OpenSSH..."
if [ -f "${ROOT_DIR}/openssh/start_ssh.sh" ]; then
    chmod +x "${ROOT_DIR}/openssh/start_ssh.sh"
    "${ROOT_DIR}/openssh/start_ssh.sh" &
fi

# --- 5. Bionic Cat Core Infrastructure (Mosquitto) ---

# Switch to module directory for relative paths inside Bionic Cat
cd "${BASE_DIR}" || {
    echo "[CRITICAL] Cannot access module directory: ${BASE_DIR}"
    exit 1
}

echo "[INFO] Starting Mosquitto..."
MOSQ_BIN="./3rd/mosquitto/sbin/mosquitto"
if [ -f "$MOSQ_BIN" ]; then
    chmod +x "$MOSQ_BIN"
    nohup "$MOSQ_BIN" > "${LOG_DIR}/mosquitto.log" 2>&1 &
    
    # Wait for Mosquitto to be ready (Critical for other modules)
    echo "[INFO] Waiting for Mosquitto to be ready..."
    TIMEOUT=10
    while [ $TIMEOUT -gt 0 ]; do
        if grep -q "mosquitto version .* running" "${LOG_DIR}/mosquitto.log" 2>/dev/null; then
            echo "[INFO] Mosquitto is running."
            break
        fi
        sleep 1
        TIMEOUT=$((TIMEOUT - 1))
    done
else
    echo "[CRITICAL] Mosquitto binary not found at $MOSQ_BIN"
fi

#--- 6. Start Application Modules ---
# 6.4 Audio
start_app "./microphone_module" "microphone_module"
start_app "./speaker_module" "speaker_module"
#start_app "./eyeball_module" "eyeball_module"
# 6.1 Core Services
start_app "./serial_adapter_module" "serial_adapter_module"

# start_app "./serial_adapter_module" "serial_adapter_module"

# # 6.2 Vision & LED
start_app "./action_controller_module" "action_controller_module" # Uncomment if needed
#start_app "./vision_module" "vision_module"
start_app "./led_contrl_module" "led_contrl_module"

start_app "./bionic_cat_core_module" "bionic_cat_core_module"
# 6.3 Radar (Requires pre-start script)
#if [ -f "./scripts/open_radar.sh" ]; then
#    echo "[INFO] Configuring Radar (open_radar.sh)..."
#    chmod +x "./scripts/open_radar.sh"
#    ./scripts/open_radar.sh
#fi
#start_app "./radar_module" "radar_module"


# 6.5 UI & Display
start_app "./eyeball_module" "eyeball_module"

# # 6.6 Web & Interaction
start_app "./bionic_cat_web_adapter" "bionic_cat_web_adapter"
start_app "./interaction_detector_module" "interaction_detector_module"

echo "[INFO] =========================================="
echo "[INFO] All Auto-Start tasks triggered."
echo "[INFO] =========================================="

exit 0
