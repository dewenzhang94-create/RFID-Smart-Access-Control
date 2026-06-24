"""
smart_cmd.py - AI 语音意图识别引擎

支持两种模式:
  1. 本地规则模式 (默认, 无需API)
  2. DeepSeek AI 模式 (设置环境变量 DEEPSEEK_API_KEY 后启用)

本地模式使用中文语义模糊匹配, 覆盖多种自然语言表达方式。
"""

import os
import json
import difflib
from typing import Optional, Tuple

try:
    from config import DEEPSEEK_API_KEY
except ImportError:
    DEEPSEEK_API_KEY = ""

# ============================================
# 意图定义 (本地规则模式)
# ============================================

# 每条意图包含: 指令名, 权重, 正面词, 负面词(排除用), 动作描述
INTENTS = [
    # ---- 开门 / 开锁 ----
    {
        "cmd": "unlock",
        "name": "远程开门",
        "weight": 0,
        "positive": [
            "开门", "开锁", "解锁", "打开门", "把门打开", "开一下门",
            "帮忙开门", "请开门", "把锁打开", "我要进来", "让我进去",
            "开下门", "门打开", "开门吧", "帮我打开", "开门进来",
        ],
        "negative": ["关门", "锁门", "不要开门", "别开门"],
        "reply": "好的，正在为您开门 "
    },
    # ---- 锁门 / 上锁 ----
    {
        "cmd": "lock",
        "name": "远程锁门",
        "weight": 0,
        "positive": [
            "锁门", "上锁", "关门", "把门锁上", "锁上门", "把门关了",
            "帮忙锁门", "请锁门", "锁起来", "把门锁好", "我要锁门",
            "门上锁", "门锁上", "锁了吧", "帮我锁一下", "关上门",
        ],
        "negative": ["开门", "解锁", "不要锁", "别锁门"],
        "reply": "好的，正在为您锁门 "
    },
    # ---- 测试报警 ----
    {
        "cmd": "alarm_test",
        "name": "测试报警",
        "weight": 0,
        "positive": [
            "报警", "测试报警", "告警", "拉响警报", "试一下报警",
            "报警器", "测试一下报警", "触发报警", "报警测试", "警告",
            "发出警报", "警报测试", "报警响了", "让报警响",
        ],
        "negative": ["停止报警", "关闭报警", "取消报警"],
        "reply": "好的，正在触发报警测试 "
    },
    # ---- 查询温度 ----
    {
        "cmd": "query_temp",
        "name": "查询温度",
        "weight": 0,
        "positive": [
            "温度", "多少度", "气温", "室内温度", "热不热", "冷吗",
            "现在几度", "温度多少", "测温", "环境温度",
        ],
        "negative": [],
        "reply": "正在查询环境温度..."
    },
    # ---- 查询湿度 ----
    {
        "cmd": "query_humidity",
        "name": "查询湿度",
        "weight": 0,
        "positive": [
            "湿度", "潮不潮", "潮湿", "干不干", "空气湿度",
            "湿度多少", "环境湿度",
        ],
        "negative": [],
        "reply": "正在查询空气湿度..."
    },
    # ---- 查询状态 ----
    {
        "cmd": "query_status",
        "name": "查询系统状态",
        "weight": 0,
        "positive": [
            "状态", "系统状态", "设备状态", "门锁状态", "门开着吗",
            "门关了没", "现在什么情况", "目前状态", "运行状态",
            "有人在吗", "有人吗", "检测到人吗", "门口有人吗",
            "在线吗", "连上了吗",
        ],
        "negative": [],
        "reply": "正在查询系统状态..."
    },
    # ---- 查询烟雾 ----
    {
        "cmd": "query_smoke",
        "name": "查询烟雾",
        "weight": 0,
        "positive": [
            "烟雾", "空气质量", "有没有烟", "抽烟", "烟气",
            "烟雾浓度", "空气好不好", "有烟吗",
        ],
        "negative": [],
        "reply": "正在查询烟雾浓度..."
    },
]


def _similarity(a: str, b: str) -> float:
    """计算两个字符串的相似度 (0~1)"""
    return difflib.SequenceMatcher(None, a, b).ratio()


def _match_intent(text: str) -> Optional[Tuple[str, str, str]]:
    """
    本地规则匹配: 返回 (cmd, name, reply) 或 None
    支持模糊匹配和部分匹配
    """
    text_lower = text.lower().strip()

    best_cmd = None
    best_name = None
    best_reply = None
    best_score = 0.0

    for intent in INTENTS:
        score = 0.0
        # 正面词匹配
        for word in intent["positive"]:
            # 完全包含
            if word in text_lower:
                score = max(score, 0.9)
            # 部分包含 (短词组)
            elif len(word) <= 4 and any(c in text_lower for c in word):
                score = max(score, 0.6)
            # 模糊匹配 (长文本)
            elif len(text_lower) >= len(word):
                sim = _similarity(text_lower[:len(word)], word)
                score = max(score, sim * 0.8)

        # 负面词惩罚
        for neg in intent["negative"]:
            if neg in text_lower:
                score -= 0.5

        if score > best_score:
            best_score = score
            best_cmd = intent["cmd"]
            best_name = intent["name"]
            best_reply = intent.get("reply", "")

    if best_score >= 0.55:
        return (best_cmd, best_name, best_reply)
    return None


# ============================================
# AI 模式 (DeepSeek)
# ============================================

def _ai_interpret(text: str, context: dict = None) -> Optional[Tuple[str, str, str]]:
    """
    使用 DeepSeek API 解释自然语言指令
    需要设置环境变量 DEEPSEEK_API_KEY
    """
    api_key = DEEPSEEK_API_KEY or os.environ.get("DEEPSEEK_API_KEY", "")
    if not api_key:
        return None

    ctx = ""
    if context:
        ctx = f"\n当前系统状态: 温度{context.get('temp','?')}°C, 湿度{context.get('hum','?')}%, 烟雾{context.get('smoke','?')}, 门{'开' if context.get('door') else '关'}, 火焰{'有' if context.get('fire') else '无'}"

    prompt = f"""你是一个智能门禁系统的语音助手。根据用户的自然语言指令，返回JSON格式的意图。

可用指令:
- unlock: 远程开门/开锁
- lock: 远程锁门/上锁
- alarm_test: 测试报警
- query_temp: 查询温度
- query_humidity: 查询湿度
- query_smoke: 查询烟雾
- query_status: 查询系统状态
- chat: 闲聊

{ctx}

用户说: "{text}"

请严格返回JSON格式: {{"cmd":"指令代码","reply":"你的回复(友好、简短、中文)","need_publish": true/false}}
只有 unlock/lock/alarm_test 需要 need_publish=true，查询类命令返回false。"""

    try:
        import requests
        resp = requests.post(
            "https://api.deepseek.com/v1/chat/completions",
            json={
                "model": "deepseek-chat",
                "messages": [{"role": "user", "content": prompt}],
                "temperature": 0.1,
                "max_tokens": 200,
            },
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {api_key}",
            },
            timeout=8,
        )
        if resp.status_code != 200:
            print(f"[AI] DeepSeek HTTP {resp.status_code}: {resp.text[:200]}")
            return None
        body = resp.json()
        content = body["choices"][0]["message"]["content"].strip()

        # 解析 JSON
        if content.startswith("```"):
            content = content.split("\n", 1)[1]
            if content.endswith("```"):
                content = content[:-3]
        result = json.loads(content)
        cmd = result.get("cmd", "chat")
        reply = result.get("reply", "")
        need_publish = result.get("need_publish", False)
        return (cmd, cmd, reply) if need_publish or cmd.startswith("query") else ("chat", "chat", reply)

    except Exception as e:
        print(f"[AI] DeepSeek error: {e}")
        return None


# ============================================
# 统一接口
# ============================================

def interpret(text: str, context: dict = None) -> dict:
    """
    解析用户语音指令 -> 返回结构化结果

    返回:
    {
        "ok": bool,
        "cmd": str,         # 指令代码
        "name": str,        # 中文名称
        "reply": str,       # 回复文本
        "mode": "ai"|"local"|"unknown"
    }
    """
    # 1. 本地规则优先 (毫秒级, 安全稳定)
    match = _match_intent(text)
    if match:
        cmd, name, reply = match
        return {"ok": True, "cmd": cmd, "name": name, "reply": reply, "mode": "local"}

    # 2. 未匹配时尝试 AI (有 key 才启用)
    if DEEPSEEK_API_KEY or os.environ.get("DEEPSEEK_API_KEY"):
        result = _ai_interpret(text, context)
        if result:
            cmd, name, reply = result
            return {"ok": True, "cmd": cmd, "name": name, "reply": reply, "mode": "ai"}

    # 3. 完全未识别
    return {
        "ok": False,
        "cmd": "unknown",
        "name": "未识别",
        "reply": "抱歉，我不太理解，请说开门、锁门、测试报警或查询状态。",
        "mode": "unknown"
    }


# ============================================
# 测试
# ============================================
if __name__ == "__main__":
    tests = [
        "帮我把门打开一下",
        "有陌生人进来了快锁门",
        "测试一下那个报警器",
        "今天室内温度多少度",
        "现在门是开着的吗",
        "空气质量怎么样",
        "把门关上行不行",
    ]
    for t in tests:
        r = interpret(t)
        print(f"  '{t}'")
        print(f"    -> {r['cmd']} | {r['reply']}  ({r['mode']})")
        print()