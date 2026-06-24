"""
database.py - SQLite 数据库操作模块
负责建表、插入日志、查询记录
"""

import sqlite3
import os
from config import DB_FILE


def get_db_path():
    """获取数据库文件绝对路径 (与脚本同目录)"""
    base = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(base, DB_FILE)


def init_db():
    """
    初始化数据库: 创建 users 和 logs 表, 插入默认用户
    """
    db_path = get_db_path()
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    # --- 用户表 ---
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS users (
            uid      TEXT PRIMARY KEY,
            username TEXT NOT NULL,
            role     TEXT DEFAULT 'user'
        )
    """)

    # --- 日志表 ---
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS logs (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            uid         TEXT,
            username    TEXT,
            event       TEXT,
            access_time DATETIME
        )
    """)

    # --- 插入默认用户 (如已存在则忽略) ---
    default_users = [
        ("63A24B9F", "张三", "admin"),
        ("A1B2C3D4", "李四", "user"),
        ("F1E2D3C4", "王五", "user"),
    ]
    cursor.executemany(
        "INSERT OR IGNORE INTO users(uid, username, role) VALUES (?, ?, ?)",
        default_users
    )

    conn.commit()
    conn.close()
    print(f"[DB] Database initialized: {db_path}")


def insert_log(uid: str, username: str, event: str, access_time: str):
    """
    插入一条认证/报警日志
    """
    db_path = get_db_path()
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute(
        "INSERT INTO logs (uid, username, event, access_time) VALUES (?, ?, ?, ?)",
        (uid, username, event, access_time)
    )
    conn.commit()
    conn.close()


def query_logs(limit: int = 50):
    """
    查询最近的日志记录
    """
    db_path = get_db_path()
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute(
        "SELECT id, uid, username, event, access_time FROM logs ORDER BY id DESC LIMIT ?",
        (limit,)
    )
    rows = cursor.fetchall()
    conn.close()
    return rows


def query_users():
    """
    查询所有授权用户
    """
    db_path = get_db_path()
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute("SELECT uid, username, role FROM users")
    rows = cursor.fetchall()
    conn.close()
    return rows


def get_log_statistics():
    """
    获取日志统计信息
    """
    db_path = get_db_path()
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    stats = {}

    # 总记录数
    cursor.execute("SELECT COUNT(*) FROM logs")
    stats["total"] = cursor.fetchone()[0]

    # 按事件类型统计
    cursor.execute(
        "SELECT event, COUNT(*) FROM logs GROUP BY event ORDER BY COUNT(*) DESC"
    )
    stats["by_event"] = cursor.fetchall()

    # 今日记录数
    cursor.execute(
        "SELECT COUNT(*) FROM logs WHERE date(access_time) = date('now')"
    )
    stats["today"] = cursor.fetchone()[0]

    conn.close()
    return stats


# ============================================
# 命令行直接运行: 初始化数据库 + 打印状态
# ============================================
if __name__ == "__main__":
    init_db()

    print("\n=== 授权用户列表 ===")
    for u in query_users():
        print(f"  UID={u[0]}  Name={u[1]}  Role={u[2]}")

    print("\n=== 统计信息 ===")
    s = get_log_statistics()
    print(f"  总记录数: {s['total']}")
    print(f"  今日记录: {s['today']}")
    print(f"  事件分布: {s['by_event']}")

    print("\n=== 最近 10 条日志 ===")
    for log in query_logs(10):
        print(f"  [{log[4]}] {log[2]} - {log[3]}")
