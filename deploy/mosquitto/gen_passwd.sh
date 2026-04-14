#!/bin/bash
# Tạo file mật khẩu Mosquitto
# Chạy trên VPS: bash gen_passwd.sh

set -e

if [ -z "$MQTT_USER" ] || [ -z "$MQTT_PASSWORD" ]; then
    echo "Cần set biến môi trường MQTT_USER và MQTT_PASSWORD"
    echo "Ví dụ: MQTT_USER=waterfall MQTT_PASSWORD=abc123 bash gen_passwd.sh"
    exit 1
fi

docker run --rm eclipse-mosquitto:2.0 \
    mosquitto_passwd -b -c /dev/stdout "$MQTT_USER" "$MQTT_PASSWORD" \
    > passwd

echo "Tạo file passwd thành công cho user: $MQTT_USER"
