#!/bin/bash
# ── Waterfall Deploy Script ────────────────────────────────────────────────
# Chạy trên VPS lần đầu: bash deploy.sh init
# Cập nhật code sau:      bash deploy.sh update
# Xem logs:               bash deploy.sh logs
# Dừng:                   bash deploy.sh stop
set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEPLOY_DIR="$REPO_DIR/deploy"
ENV_FILE="$DEPLOY_DIR/.env"

# ── Màu sắc output ──────────────────────────────────────────────────────────
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[✓]${NC} $1"; }
warn()  { echo -e "${YELLOW}[!]${NC} $1"; }
error() { echo -e "${RED}[✗]${NC} $1"; exit 1; }

# ── Kiểm tra .env ───────────────────────────────────────────────────────────
check_env() {
    [ -f "$ENV_FILE" ] || error ".env không tồn tại. Chạy: cp .env.example .env rồi điền thông tin"
    source "$ENV_FILE"
    [ -n "$DOMAIN" ]        || error "DOMAIN chưa được set trong .env"
    [ -n "$SSL_EMAIL" ]     || error "SSL_EMAIL chưa được set trong .env"
    [ -n "$MQTT_USER" ]     || error "MQTT_USER chưa được set trong .env"
    [ -n "$MQTT_PASSWORD" ] || error "MQTT_PASSWORD chưa được set trong .env"
    info "File .env hợp lệ"
}

# ── Cài Docker nếu chưa có ──────────────────────────────────────────────────
install_docker() {
    if ! command -v docker &>/dev/null; then
        warn "Docker chưa được cài. Đang cài..."
        curl -fsSL https://get.docker.com | sh
        systemctl enable docker && systemctl start docker
        info "Docker đã cài xong"
    else
        info "Docker đã có: $(docker --version)"
    fi

    if ! command -v docker &>/dev/null || ! docker compose version &>/dev/null; then
        warn "Docker Compose plugin chưa có. Đang cài..."
        apt-get install -y docker-compose-plugin
    fi
    info "Docker Compose: $(docker compose version)"
}

# ── Tạo file passwd Mosquitto ────────────────────────────────────────────────
setup_mosquitto_passwd() {
    local passwd_file="$DEPLOY_DIR/mosquitto/passwd"
    if [ ! -f "$passwd_file" ]; then
        info "Tạo Mosquitto password file..."
        mkdir -p "$DEPLOY_DIR/mosquitto"
        docker run --rm \
            -v "$DEPLOY_DIR/mosquitto:/mosquitto/config" \
            eclipse-mosquitto:2.0 \
            mosquitto_passwd -b -c /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASSWORD"
        chmod 600 "$passwd_file"
        info "Passwd file tạo xong: $passwd_file"
    else
        info "Passwd file đã tồn tại"
    fi
}

# ── Copy cert Mosquitto từ Let's Encrypt ─────────────────────────────────────
sync_mqtt_certs() {
    local cert_src="/etc/letsencrypt/live/$DOMAIN"
    local cert_dst="$DEPLOY_DIR/mosquitto/certs"
    mkdir -p "$cert_dst"

    if [ -d "$cert_src" ]; then
        cp "$cert_src/fullchain.pem" "$cert_dst/"
        cp "$cert_src/privkey.pem"   "$cert_dst/"
        cp "$cert_src/chain.pem"     "$cert_dst/"
        chmod 644 "$cert_dst"/*.pem
        info "Cert MQTT đã copy"
    else
        warn "Chưa có cert Let's Encrypt. Cert sẽ được tạo khi chạy certbot"
    fi
}

# ── Lấy SSL cert lần đầu (HTTP challenge) ───────────────────────────────────
init_ssl() {
    info "Lấy SSL cert (standalone mode — Certbot tự mở port 80)..."
    # Dừng bất kỳ container nào đang chiếm port 80
    docker stop nginx-tmp 2>/dev/null || true

    docker run --rm \
        -v certbot_certs:/etc/letsencrypt \
        -p 80:80 \
        certbot/certbot certonly \
        --standalone \
        --email "$SSL_EMAIL" --agree-tos --no-eff-email \
        -d "$DOMAIN"

    info "SSL cert đã lấy thành công"
}

# ── INIT: Lần đầu deploy ─────────────────────────────────────────────────────
cmd_init() {
    info "=== Khởi tạo lần đầu ==="
    check_env
    install_docker
    setup_mosquitto_passwd

    cd "$DEPLOY_DIR"

    # Build và pull images
    info "Build Docker images..."
    docker compose build --no-cache

    # Lấy SSL cert
    init_ssl
    sync_mqtt_certs

    # Thay DOMAIN trong nginx config
    sed -i "s/\${DOMAIN}/$DOMAIN/g" "$DEPLOY_DIR/nginx/conf.d/waterfall.conf"

    # Start tất cả services
    info "Khởi động services..."
    docker compose --env-file "$ENV_FILE" up -d

    info "=== Deploy hoàn tất! ==="
    echo ""
    echo "  Web app:  https://$DOMAIN"
    echo "  MQTT:     mqtts://$DOMAIN:8883"
    echo ""
    echo "Lệnh hữu ích:"
    echo "  bash deploy.sh logs     — xem logs"
    echo "  bash deploy.sh update   — cập nhật code"
    echo "  bash deploy.sh status   — trạng thái services"
}

# ── UPDATE: Cập nhật code ────────────────────────────────────────────────────
cmd_update() {
    info "=== Cập nhật code ==="
    check_env
    cd "$REPO_DIR"

    # Pull code mới (nếu dùng git)
    if [ -d .git ]; then
        git pull origin main
        info "Git pull xong"
    fi

    cd "$DEPLOY_DIR"
    docker compose --env-file "$ENV_FILE" build backend
    docker compose --env-file "$ENV_FILE" up -d --no-deps backend
    info "Backend đã cập nhật"
}

# ── LOGS ─────────────────────────────────────────────────────────────────────
cmd_logs() {
    cd "$DEPLOY_DIR"
    docker compose logs -f --tail=100 "${2:-}"
}

# ── STATUS ────────────────────────────────────────────────────────────────────
cmd_status() {
    cd "$DEPLOY_DIR"
    docker compose ps
    echo ""
    source "$ENV_FILE"
    echo "Web:  https://$DOMAIN"
    echo "MQTT: mqtts://$DOMAIN:8883"
}

# ── STOP ──────────────────────────────────────────────────────────────────────
cmd_stop() {
    cd "$DEPLOY_DIR"
    docker compose down
    info "Tất cả services đã dừng"
}

# ── RENEW SSL ─────────────────────────────────────────────────────────────────
cmd_renew_ssl() {
    cd "$DEPLOY_DIR"
    docker compose run --rm certbot renew
    sync_mqtt_certs
    docker compose exec nginx nginx -s reload
    info "SSL cert đã renew"
}

# ── Main ──────────────────────────────────────────────────────────────────────
case "${1:-help}" in
    init)      cmd_init    ;;
    update)    cmd_update  ;;
    logs)      cmd_logs    ;;
    status)    cmd_status  ;;
    stop)      cmd_stop    ;;
    renew-ssl) cmd_renew_ssl ;;
    *)
        echo "Waterfall Deploy Script"
        echo ""
        echo "Usage: bash deploy.sh <command>"
        echo ""
        echo "Commands:"
        echo "  init       — Lần đầu deploy (cài Docker, lấy SSL, start services)"
        echo "  update     — Cập nhật code backend"
        echo "  logs       — Xem logs real-time"
        echo "  status     — Trạng thái services"
        echo "  stop       — Dừng tất cả"
        echo "  renew-ssl  — Gia hạn SSL cert thủ công"
        ;;
esac
