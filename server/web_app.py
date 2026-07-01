"""
web_app.py - RFID 智能门禁 Web 管理系统
Flask + Bootstrap5 + Chart.js
角色: admin(管理员) / user(普通用户)
页面: 登录注册 / 实时监控 / 远程控制 / 数据分析 / 权限管理
启动: python web_app.py
"""

import json
import time
import threading
import hashlib
from datetime import datetime

from flask import (Flask, render_template, request, jsonify,
                   redirect, url_for, session, g)
import sqlite3
import os
import paho.mqtt.client as mqtt

# --- 配置 ---
from config import MQTT_BROKER, MQTT_PORT, MQTT_TOPICS, DB_FILE
from analysis_engine import engine as analysis_engine
from smart_cmd import interpret as smart_interpret

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(BASE_DIR, DB_FILE)
SECRET_KEY = 'rfid_gate_system_2026_secret'
MQTT_TOPIC_CMD = 'rfid/cmd'

app = Flask(__name__)
app.secret_key = SECRET_KEY

# HTTPS 安全 Cookie 配置
app.config['SESSION_COOKIE_SECURE'] = True   # 仅通过 HTTPS 传输 session cookie
app.config['SESSION_COOKIE_HTTPONLY'] = True  # 禁止 JavaScript 读取 cookie，防 XSS

# 禁用缓存 (确保前端始终获取最新版本)
@app.after_request
def no_cache(response):
    response.headers['Cache-Control'] = 'no-store, no-cache, must-revalidate, max-age=0'
    response.headers['Pragma'] = 'no-cache'
    response.headers['Expires'] = '0'
    return response

# ============================================
# 实时数据缓存 (MQTT → Web 页面)
# ============================================
live_data = {
    'logs': [],          # 最近 50 条日志
    'door_open': False,
    'last_card': None,
    'alarm': None,
    'temp': None,
    'humidity': None,
    'smoke': None,
    'fire': False,
    'human': False,
    'online': False,     # ESP32 是否在线
    'last_heartbeat': '',
}
live_lock = threading.RLock()  # 可重入锁，防止 card_learned→publish_cmd 死锁


# ============================================
# 数据库操作
# ============================================

def get_db():
    if 'db' not in g:
        g.db = sqlite3.connect(DB_PATH)
        g.db.row_factory = sqlite3.Row
        g.db.execute('PRAGMA journal_mode=WAL')
    return g.db


@app.teardown_appcontext
def close_db(exception):
    db = g.pop('db', None)
    if db:
        db.close()


def init_user_table():
    """创建用户账户表"""
    db_path = os.path.join(BASE_DIR, DB_FILE)
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS accounts (
            id       INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT    UNIQUE NOT NULL,
            password TEXT    NOT NULL,
            role     TEXT    DEFAULT 'user',
            created  DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    # 插入默认管理员 (密码: admin123)
    pw = hashlib.sha256('admin123'.encode()).hexdigest()
    c.execute(
        "INSERT OR IGNORE INTO accounts(username, password, role) VALUES (?, ?, ?)",
        ('admin', pw, 'admin')
    )
    # 默认普通用户 (密码: user123)
    pw2 = hashlib.sha256('user123'.encode()).hexdigest()
    c.execute(
        "INSERT OR IGNORE INTO accounts(username, password, role) VALUES (?, ?, ?)",
        ('user', pw2, 'user')
    )
    conn.commit()
    conn.close()


def hash_pw(password: str) -> str:
    return hashlib.sha256(password.encode()).hexdigest()


# ============================================
# MQTT 实时监听线程
# ============================================

_mqtt_persistent_client = None  # 持久连接, 供 publish_cmd 复用

def mqtt_on_connect(client, userdata, flags, reason_code, properties=None):
    global _mqtt_persistent_client
    _mqtt_persistent_client = client
    print(f"[Web] MQTT connected")
    for topic in MQTT_TOPICS:
        client.subscribe(topic)
    client.subscribe("rfid/cmd")


def mqtt_on_message(client, userdata, msg):
    # Handle sync_users command from ESP32
    if msg.topic == "rfid/cmd" and msg.payload.decode('utf-8', errors='replace').strip() == "sync_users":
        print(f"[Web] ESP32 requested user sync, publishing full list...")
        publish_user_sync()
        return

    try:
        raw = msg.payload.decode('utf-8')
        # 尝试 AES 解密（如果是加密数据）
        from crypto_helper import aes_decrypt
        is_encrypted = all(c in '0123456789ABCDEFabcdef' for c in raw) and len(raw) > 0
        if is_encrypted:
            plain = aes_decrypt(raw)
            if plain:
                payload = plain
            else:
                payload = raw
        else:
            payload = raw

        data = json.loads(payload)
    except:
        data = {'event': 'unknown', 'raw': msg.payload.decode('utf-8', errors='replace')[:100]}

    with live_lock:
        event = data.get('event', 'unknown')
        uid = data.get('uid', '')
        username = data.get('username', '')
        access_time = data.get('time', datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

        entry = {
            'topic': msg.topic,
            'uid': uid,
            'username': username,
            'event': event,
            'time': access_time,
            'temp': data.get('temp'),
            'humidity': data.get('humidity'),
            'smoke': data.get('smoke'),
            'fire': data.get('fire'),
            'human': data.get('human'),
            'raw': data.get('raw', raw),  # 原始 payload（加密十六进制或明文）
            'direction': 'up',            # ESP32 → 服务器
        }
        live_data['logs'].insert(0, entry)
        if len(live_data['logs']) > 50:
            live_data['logs'] = live_data['logs'][:50]

        # 更新实时状态
        if event == 'login':
            live_data['last_card'] = entry
        elif event == 'door_open':
            live_data['door_open'] = True
        elif event == 'door_close':
            live_data['door_open'] = False
        elif event in ('unauthorized', 'illegal_door'):
            live_data['alarm'] = entry
        elif event == 'heartbeat':
            live_data['online'] = True
            live_data['last_heartbeat'] = access_time
            if data.get('temp') is not None:
                live_data['temp'] = data['temp']
                live_data['humidity'] = data['humidity']
                live_data['smoke'] = data['smoke']
                live_data['fire'] = data.get('fire', False)
                live_data['human'] = data.get('human', False)
            # 从心跳同步门锁状态 (每30秒自愈)
            if data.get('door') is not None:
                live_data['door_open'] = data['door'] if isinstance(data['door'], bool) else (data['door'] == 'true')
        elif event == 'system_start':
            live_data['online'] = True
        elif event == 'human_detected':
            live_data['human'] = True
        elif event == 'human_left':
            live_data['human'] = False
        elif event == 'card_learned':
            new_uid  = data.get('uid', '')
            new_name = data.get('username', '')
            if new_uid and new_name:
                try:
                    _db = sqlite3.connect(DB_PATH)
                    _db.execute(
                        "INSERT OR REPLACE INTO users (uid, username, role) VALUES (?, ?, 'user')",
                        (new_uid, new_name)
                    )
                    _db.commit(); _db.close()
                    live_data['learn_result'] = {'ok': True, 'uid': new_uid, 'name': new_name}
                    publish_cmd(f"sync_user_add:{new_uid}:{new_name}")
                except Exception as ex:
                    live_data['learn_result'] = {'ok': False, 'error': str(ex)}

        # 实时写入数据库 (心跳不写库, 避免日志膨胀)
        if event != 'heartbeat':
            try:
                _db = sqlite3.connect(DB_PATH)
                _db.execute(
                    "INSERT INTO logs (uid, username, event, access_time) VALUES (?, ?, ?, ?)",
                    (uid, username, event, access_time)
                )
                _db.commit()
                _db.close()
            except:
                pass


def mqtt_thread():
    """后台 MQTT 监听线程"""
    retries = 0
    while True:
        try:
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                                 client_id='RFID_Web_Dashboard')
            client.on_connect = mqtt_on_connect
            client.on_message = mqtt_on_message
            client.connect(MQTT_BROKER, MQTT_PORT, 60)
            print(f"[Web] MQTT listener started, broker={MQTT_BROKER}")
            retries = 0
            client.loop_forever()
        except Exception as e:
            retries += 1
            delay = min(retries * 5, 60)
            print(f"[Web] MQTT error: {e}, retrying in {delay}s...")
            time.sleep(delay)


# ============================================
# MQTT 发送指令
# ============================================

def publish_cmd(cmd: str):
    """发送控制指令到 ESP32 通过 MQTT"""
    import paho.mqtt.publish as publish
    try:
        publish.single("rfid/cmd", cmd, hostname=MQTT_BROKER, port=MQTT_PORT)
        print(f"[Web] MQTT cmd published: {cmd}")
        # 记录 outgoing 消息到 MQTT 监控
        with live_lock:
            live_data['logs'].insert(0, {
                'topic': 'rfid/cmd', 'uid': '', 'username': 'SERVER',
                'event': 'cmd_sent', 'time': datetime.now().strftime('%Y-%m-%dT%H:%M:%S'),
                'temp': None, 'humidity': None, 'smoke': None, 'fire': False, 'human': False,
                'raw': cmd, 'direction': 'down'
            })
            if len(live_data['logs']) > 50:
                live_data['logs'] = live_data['logs'][:50]
        return True
    except Exception as e:
        print(f"[Web] MQTT publish error: {e}")
        return False


def publish_user_sync():
    """下发完整白名单到 ESP32 (通过 rfid/users topic)"""
    import paho.mqtt.publish as publish
    try:
        conn = sqlite3.connect(DB_PATH)
        rows = conn.execute("SELECT uid, username FROM users ORDER BY uid").fetchall()
        conn.close()
        users = [{"uid": r[0], "username": r[1]} for r in rows]
        payload = json.dumps(users, ensure_ascii=False)
        publish.single("rfid/users", payload, hostname=MQTT_BROKER, port=MQTT_PORT)
        print(f"[Web] User sync published: {len(users)} users")
        # 记录 outgoing 消息到 MQTT 监控
        with live_lock:
            live_data['logs'].insert(0, {
                'topic': 'rfid/users', 'uid': '', 'username': 'SERVER',
                'event': 'users_sync', 'time': datetime.now().strftime('%Y-%m-%dT%H:%M:%S'),
                'temp': None, 'humidity': None, 'smoke': None, 'fire': False, 'human': False,
                'raw': f'{len(users)} users synced', 'direction': 'down'
            })
            if len(live_data['logs']) > 50:
                live_data['logs'] = live_data['logs'][:50]
        return True
    except Exception as e:
        print(f"[Web] User sync error: {e}")
        return False


# ============================================
# 登录辅助
# ============================================

def login_required(func):
    """登录检查装饰器"""
    from functools import wraps

    @wraps(func)
    def wrapper(*args, **kwargs):
        if 'user_id' not in session:
            return redirect(url_for('login_page'))
        return func(*args, **kwargs)
    return wrapper


def admin_required(func):
    """管理员检查装饰器"""
    from functools import wraps

    @wraps(func)
    def wrapper(*args, **kwargs):
        if 'user_id' not in session:
            return redirect(url_for('login_page'))
        if session.get('role') != 'admin':
            return render_template('error.html', msg='需要管理员权限'), 403
        return func(*args, **kwargs)
    return wrapper


# ============================================
# 路由: 登录 & 注册
# ============================================

@app.route('/')
def index():
    return app.send_static_file('spa.html')


@app.route('/login')
def login_page():
    return render_template('login.html')


@app.route('/api/login', methods=['POST'])
def api_login():
    data = request.get_json()
    username = data.get('username', '')
    password = data.get('password', '')

    db = get_db()
    user = db.execute(
        "SELECT * FROM accounts WHERE username = ?", (username,)
    ).fetchone()

    if user and user['password'] == password:
        session['user_id'] = user['id']
        session['username'] = user['username']
        session['role'] = user['role']
        return jsonify({'ok': True, 'role': user['role'],
                        'username': user['username']})

    return jsonify({'ok': False, 'error': '用户名或密码错误'}), 401


@app.route('/api/register', methods=['POST'])
def api_register():
    data = request.get_json()
    username = data.get('username', '').strip()
    password = data.get('password', '').strip()

    if len(username) < 3 or len(password) < 6:
        return jsonify({'ok': False, 'error': '用户名至少3位，密码至少6位'}), 400

    db = get_db()
    exist = db.execute(
        "SELECT id FROM accounts WHERE username = ?", (username,)
    ).fetchone()
    if exist:
        return jsonify({'ok': False, 'error': '用户名已存在'}), 400

    db.execute(
        "INSERT INTO accounts (username, password, role) VALUES (?, ?, ?)",
        (username, password, 'user')
    )
    db.commit()
    return jsonify({'ok': True, 'msg': '注册成功，请重新登录'})


@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('login_page'))


# ============================================
# 路由: 仪表盘 (管理员/用户通用)
# ============================================

@app.route('/dashboard')
@login_required
def dashboard():
    return app.send_static_file('spa.html')


# ============================================
# 路由: 实时监控
# ============================================

@app.route('/monitor')
@login_required
def monitor():
    return app.send_static_file('spa.html')


@app.route('/api/live')
@login_required
def api_live():
    """获取实时数据 (含离线检测: 超65秒没收心跳则判定离线). 支持 ?limit=N (1-50, 默认10)"""
    limit = request.args.get('limit', 10, type=int)
    limit = max(1, min(50, limit))
    with live_lock:
        data = dict(live_data)
        data['logs'] = list(data['logs'][:limit])
        if data.get('last_heartbeat'):
            try:
                hb = datetime.strptime(data['last_heartbeat'], '%Y-%m-%dT%H:%M:%S')
                if (datetime.now() - hb).total_seconds() > 65:
                    data['online'] = False
            except:
                pass
    return jsonify(data)


# ============================================
# 路由: 远程控制 (管理员)
# ============================================

@app.route('/control')
@login_required
def control():
    return app.send_static_file('spa.html')


@app.route('/api/cmd', methods=['POST'])
@login_required
def api_cmd():
    """发送控制指令"""
    data = request.get_json()
    cmd = data.get('cmd', '')

    if cmd == 'unlock' and session['role'] != 'admin':
        return jsonify({'ok': False, 'error': '仅管理员可远程开门'}), 403

    ok = publish_cmd(cmd)
    return jsonify({'ok': ok})


@app.route('/api/smart_cmd', methods=['POST'])
def api_smart_cmd():
    """AI 语音指令解析"""
    data = request.get_json()
    text = data.get('text', '').strip()
    if not text:
        return jsonify({'ok': False, 'reply': '没有收到语音内容'})

    # 获取当前上下文
    with live_lock:
        ctx = {
            'temp': live_data.get('temp'),
            'hum': live_data.get('humidity'),
            'smoke': live_data.get('smoke'),
            'fire': live_data.get('fire', False),
            'door': live_data.get('door_open', False),
            'online': live_data.get('online', False),
            'human': live_data.get('human', False),
        }

    # AI 解析
    result = smart_interpret(text, ctx)
    cmd = result['cmd']
    reply = result['reply']
    mode = result.get('mode', 'local')

    # 查询类指令: 用实时数据生成回复
    if cmd.startswith('query'):
        if cmd == 'query_temp':
            t = live_data.get('temp')
            reply = f"当前室内温度 {t:.1f} 摄氏度" if t is not None else "暂无温度数据"
        elif cmd == 'query_humidity':
            h = live_data.get('humidity')
            reply = f"当前相对湿度 {h:.1f}%" if h is not None else "暂无湿度数据"
        elif cmd == 'query_smoke':
            s = live_data.get('smoke')
            reply = f"当前烟雾值为 {s}" if s is not None else "暂无烟雾数据"
        elif cmd == 'query_status':
            door = "已开启" if live_data.get('door_open') else "已关闭"
            fire = "有火情!" if live_data.get('fire') else "安全"
            human = "检测到人员" if live_data.get('human') else "无人靠近"
            online = "在线" if live_data.get('online') else "已离线"
            t = live_data.get('temp')
            h = live_data.get('humidity')
            reply = f"设备{online}，门锁{door}，火焰{fire}，{human}"
            if t is not None:
                reply += f"，温度{t:.1f}度"
            if h is not None:
                reply += f"，湿度{h:.1f}%"
        return jsonify({'ok': True, 'cmd': cmd, 'reply': reply, 'mode': mode, 'action': False})

    # 闲聊类
    if cmd == 'chat':
        return jsonify({'ok': True, 'cmd': cmd, 'reply': reply, 'mode': mode, 'action': False})

    # 执行类指令: 必须登录 + 权限检查 + MQTT 下发
    if 'user_id' not in session:
        return jsonify({'ok': False, 'reply': '请先登录后再控制设备', 'action': False, 'need_login': True})

    if cmd in ('unlock', 'lock') and session.get('role') != 'admin':
        return jsonify({'ok': False, 'reply': '抱歉，仅管理员可以远程控制门锁', 'action': False})

    ok = publish_cmd(cmd)
    return jsonify({'ok': ok, 'cmd': cmd, 'reply': reply, 'mode': mode, 'action': True})


# ============================================
# 路由: 数据分析
# ============================================

@app.route('/analysis')
@login_required
def analysis():
    return app.send_static_file('spa.html')


@app.route('/analysis/smart')
@login_required
def smart_analysis():
    return app.send_static_file('spa.html')


@app.route('/api/smart_analysis')
@login_required
def api_smart_analysis():
    """生成智能环境分析报告"""
    with live_lock:
        temp = live_data.get('temp')
        hum = live_data.get('humidity')
        smoke = live_data.get('smoke')
        fire = live_data.get('fire', False)
        ts = live_data.get('last_heartbeat', '')

    if temp is None and smoke is None:
        # 没有传感器数据，从最近的心跳日志里取
        db = get_db()
        recent = db.execute(
            "SELECT event, access_time FROM logs WHERE event='heartbeat' "
            "ORDER BY id DESC LIMIT 1"
        ).fetchone()
        if recent:
            ts = recent['access_time']

    report = analysis_engine.generate_report(
        temp=temp, hum=hum, smoke=smoke,
        fire=fire, timestamp=ts
    )

    return jsonify({
        'temperature': temp,
        'humidity': hum,
        'smoke': smoke,
        'fire': fire,
        'timestamp': ts,
        'summary': report.summary,
        'insights': [
            {
                'level': i.level,
                'title': i.title,
                'detail': i.detail,
                'suggestion': i.suggestion,
                'reference': i.reference
            }
            for i in report.insights
        ]
    })


@app.route('/api/stats')
@login_required
def api_stats():
    """获取统计数据"""
    db = get_db()
    stats = {}

    # 今日统计
    today = datetime.now().strftime('%Y-%m-%d')
    r = db.execute(
        "SELECT COUNT(*) FROM logs WHERE date(access_time) = ?", (today,)
    ).fetchone()
    stats['today_total'] = r[0]

    r = db.execute(
        "SELECT COUNT(*) FROM logs WHERE date(access_time) = ? AND event='login'",
        (today,)
    ).fetchone()
    stats['today_pass'] = r[0]

    r = db.execute(
        "SELECT COUNT(*) FROM logs WHERE date(access_time) = ? AND event='unauthorized'",
        (today,)
    ).fetchone()
    stats['today_fail'] = r[0]

    # 最近7天趋势
    stats['daily'] = []
    for i in range(6, -1, -1):
        d = datetime.fromtimestamp(time.time() - i * 86400).strftime('%Y-%m-%d')
        r = db.execute(
            "SELECT COUNT(*) FROM logs WHERE date(access_time) = ?", (d,)
        ).fetchone()
        stats['daily'].append({'date': d[5:], 'count': r[0]})

    # 事件分布
    r = db.execute(
        "SELECT event, COUNT(*) as cnt FROM logs GROUP BY event ORDER BY cnt DESC"
    ).fetchall()
    stats['events'] = [{'event': row['event'], 'count': row['cnt']} for row in r]

    # 最近用户
    r = db.execute(
        "SELECT username, COUNT(*) as cnt FROM logs "
        "WHERE username != '' AND username != 'SYSTEM' "
        "GROUP BY username ORDER BY cnt DESC LIMIT 10"
    ).fetchall()
    stats['users'] = [{'name': row['username'], 'count': row['cnt']} for row in r]

    # 总数
    r = db.execute("SELECT COUNT(*) FROM logs").fetchone()
    stats['total'] = r[0]

    # 报警次数
    r = db.execute(
        "SELECT COUNT(*) FROM logs WHERE event IN ('unauthorized','illegal_door')"
    ).fetchone()
    stats['total_alarm'] = r[0]

    return jsonify(stats)


# ============================================
# 路由: 权限管理 (仅管理员)
# ============================================

@app.route('/admin/users')
@admin_required
def admin_users():
    return app.send_static_file('spa.html')


@app.route('/api/admin/users')
@admin_required
def api_users():
    """获取所有用户"""
    db = get_db()
    users = db.execute(
        "SELECT id, username, role, created FROM accounts ORDER BY id"
    ).fetchall()
    return jsonify([dict(u) for u in users])


@app.route('/api/admin/user', methods=['POST'])
@admin_required
def api_add_user():
    """添加用户"""
    data = request.get_json()
    username = data.get('username', '').strip()
    password = data.get('password', '').strip()
    role = data.get('role', 'user')

    if len(username) < 3 or len(password) < 6:
        return jsonify({'ok': False, 'error': '用户名至少3位，密码至少6位'})

    db = get_db()
    try:
        db.execute(
            "INSERT INTO accounts (username, password, role) VALUES (?, ?, ?)",
            (username, password, role)
        )
        db.commit()
        return jsonify({'ok': True})
    except sqlite3.IntegrityError:
        return jsonify({'ok': False, 'error': '用户名已存在'})


@app.route('/api/admin/user/<int:uid>', methods=['DELETE'])
@admin_required
def api_delete_user(uid):
    """删除用户 (不能删除自己, 不能删除超级管理员)"""
    if uid == session['user_id']:
        return jsonify({'ok': False, 'error': '不能删除自己'})
    db = get_db()
    row = db.execute("SELECT username FROM accounts WHERE id = ?", (uid,)).fetchone()
    if row and row['username'] == 'zdw':
        return jsonify({'ok': False, 'error': '超级管理员不可删除'})
    db.execute("DELETE FROM accounts WHERE id = ?", (uid,))
    db.commit()
    return jsonify({'ok': True})


@app.route('/api/admin/user/<int:uid>/role', methods=['PUT'])
@admin_required
def api_update_role(uid):
    """修改角色 (超级管理员不可降级)"""
    if uid == session['user_id']:
        return jsonify({'ok': False, 'error': '不能修改自己的角色'})
    db = get_db()
    row = db.execute("SELECT username FROM accounts WHERE id = ?", (uid,)).fetchone()
    if row and row['username'] == 'zdw':
        return jsonify({'ok': False, 'error': '超级管理员角色不可修改'})
    data = request.get_json()
    role = data.get('role', 'user')
    db.execute("UPDATE accounts SET role = ? WHERE id = ?", (role, uid))
    db.commit()
    return jsonify({'ok': True})


# ============================================
# 路由: RFID 卡管理
# ============================================

@app.route('/api/admin/cards')
@admin_required
def api_cards():
    """获取所有授权的 RFID 卡"""
    db = get_db()
    cards = db.execute("SELECT * FROM users ORDER BY uid").fetchall()
    return jsonify([dict(c) for c in cards])


@app.route('/api/admin/card', methods=['POST'])
@admin_required
def api_add_card():
    """添加授权卡"""
    data = request.get_json()
    uid = data.get('uid', '').strip().upper()
    username = data.get('username', '').strip()

    if not uid or not username:
        return jsonify({'ok': False, 'error': 'UID和用户名不能为空'})

    db = get_db()
    try:
        db.execute(
            "INSERT OR REPLACE INTO users (uid, username, role) VALUES (?, ?, 'user')",
            (uid, username)
        )
        db.commit()
        # Push to ESP32: add single user
        publish_cmd(f"sync_user_add:{uid}:{username}")
        return jsonify({'ok': True})
    except Exception as e:
        return jsonify({'ok': False, 'error': str(e)})


@app.route('/api/admin/card/<uid>', methods=['DELETE'])
@admin_required
def api_delete_card(uid):
    """删除授权卡 (超级管理员卡不可删除)"""
    if uid.upper() == 'FE320102':
        return jsonify({'ok': False, 'error': '超级管理员卡片不可删除'})
    db = get_db()
    db.execute("DELETE FROM users WHERE uid = ?", (uid,))
    db.commit()
    # Push to ESP32: delete single user
    publish_cmd(f"sync_user_del:{uid}")
    return jsonify({'ok': True})


@app.route('/api/admin/learn_card', methods=['POST'])
@admin_required
def api_learn_card():
    """刷卡录入: 前端输入名字 -> ESP32 等待刷卡"""
    data = request.get_json()
    name = data.get('name', '').strip()
    if not name:
        return jsonify({'ok': False, 'error': '名称不能为空'})
    with live_lock:
        live_data.pop('learn_result', None)
    ok = publish_cmd(f"learn:{name}")
    return jsonify({'ok': ok, 'status': 'waiting'})


@app.route('/api/admin/learn_status')
@admin_required
def api_learn_status():
    """轮询刷卡录入结果"""
    with live_lock:
        result = live_data.pop('learn_result', None)
    if result:
        return jsonify({'ok': True, 'done': True, 'uid': result.get('uid'), 'name': result.get('name')})
    return jsonify({'ok': True, 'done': False})


# ============================================
# 路由: 日志查询
# ============================================

@app.route('/api/logs')
@login_required
def api_logs():
    """查询日志 (支持分页)"""
    page = request.args.get('page', 1, type=int)
    per_page = request.args.get('per_page', 20, type=int)
    event = request.args.get('event', '')
    username = request.args.get('username', '')
    date_from = request.args.get('from', '')
    date_to = request.args.get('to', '')

    db = get_db()
    where = []
    params = []

    if event:
        where.append("event = ?")
        params.append(event)
    if username:
        where.append("username LIKE ?")
        params.append(f'%{username}%')
    if date_from:
        where.append("date(access_time) >= ?")
        params.append(date_from)
    if date_to:
        where.append("date(access_time) <= ?")
        params.append(date_to)

    sql = "SELECT * FROM logs"
    if where:
        sql += " WHERE " + " AND ".join(where)
    sql += " ORDER BY id DESC LIMIT ? OFFSET ?"
    params.extend([per_page, (page - 1) * per_page])

    rows = db.execute(sql, params).fetchall()

    # 总数
    count_sql = "SELECT COUNT(*) FROM logs"
    if where:
        count_sql += " WHERE " + " AND ".join(where)
    total = db.execute(count_sql, params[:-2]).fetchone()[0]

    return jsonify({
        'logs': [dict(r) for r in rows],
        'total': total,
        'page': page,
        'per_page': per_page,
        'pages': max(1, (total + per_page - 1) // per_page)
    })


# ============================================
# SPA 入口 (Vue3 + Element Plus)
# ============================================



@app.route('/spa')
def spa_app():
    return app.send_static_file('spa.html')


# ============================================
# 启动
# ============================================

if __name__ == '__main__':
    from database import init_db
    init_db()
    init_user_table()

    print("=" * 55)
    print("  RFID Web Dashboard")
    print(f"  管理员: admin / admin123")
    print(f"  普通用户: user / user123")
    print(f"  MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print("=" * 55)

    # 启动 MQTT 后台线程
    t = threading.Thread(target=mqtt_thread, daemon=True)
    t.start()
    time.sleep(1)

    app.run(host='0.0.0.0', port=5000, debug=False,
            ssl_context=('cert.pem', 'key.pem'))