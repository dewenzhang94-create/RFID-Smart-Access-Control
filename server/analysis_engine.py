"""
analysis_engine.py - 智能环境数据分析引擎
根据国家标准和行业规范，对环境传感器数据进行智能解读和建议
"""

from dataclasses import dataclass, field
from typing import List, Optional


@dataclass
class Insight:
    """单条分析结论"""
    level: str        # info / warning / danger / good
    title: str        # 简短标题
    detail: str       # 详细解读
    suggestion: str   # 行动建议
    reference: str    # 参考标准


@dataclass
class AnalysisReport:
    """完整分析报告"""
    temperature: Optional[float]
    humidity: Optional[float]
    smoke: Optional[int]
    fire: bool
    timestamp: str
    insights: List[Insight] = field(default_factory=list)
    summary: str = ""


class AnalysisEngine:
    """
    环境数据分析引擎
    所有阈值参考国家标准 GB 系列:
      - GB/T 18883-2022 《室内空气质量标准》
      - GB 50325-2020 《民用建筑工程室内环境污染控制标准》
      - GB 50736-2012 《民用建筑供暖通风与空气调节设计规范》
    """

    # === 温度阈值 (GB 50736-2012) ===
    TEMP_HOT         = 33.0   # 酷热, 需强制降温
    TEMP_UNCOMFORTABLE = 30.0  # 不适
    TEMP_WARM        = 28.0   # 偏热
    TEMP_COMFORT_MAX = 26.0   # 舒适上限
    TEMP_COMFORT_MIN = 18.0   # 舒适下限
    TEMP_COLD        = 16.0   # 寒冷

    # === 湿度阈值 (GB/T 18883-2022) ===
    HUM_HIGH         = 70.0   # 过高 (易滋生细菌)
    HUM_COMFORT_MAX  = 60.0   # 舒适上限
    HUM_COMFORT_MIN  = 30.0   # 舒适下限
    HUM_LOW          = 20.0   # 干燥

    # === 烟雾阈值 (MQ-2 模拟值 0-4095) ===
    SMOKE_DANGER     = 3000   # 危险浓度
    SMOKE_HIGH       = 2000   # 偏高
    SMOKE_NORMAL     = 1000   # 正常范围

    # === 甲醛阈值 (预留扩展, GB 50325-2020) ===
    FORMALDEHYDE_I_CLASS = 0.07  # I类民用建筑 ≤0.07 mg/m³ (住宅/学校/医院)
    FORMALDEHYDE_II_CLASS = 0.08  # II类民用建筑 ≤0.08 mg/m³ (办公/商业)
    FORMALDEHYDE_ELDERLY = 0.06   # 老人儿童建议值

    # ============================================
    # 温度分析
    # ============================================

    def analyze_temperature(self, temp: Optional[float]) -> List[Insight]:
        if temp is None:
            return []

        insights = []

        if temp >= self.TEMP_HOT:
            insights.append(Insight(
                level='danger',
                title=f'🔥 温度极高: {temp:.1f}°C',
                detail=(
                    f'当前室内温度 {temp:.1f}°C，远超人体舒适范围。'
                    '根据 GB 50736-2012《民用建筑供暖通风与空气调节设计规范》，'
                    '夏季空调室内设计温度宜为 24~26°C。'
                    '高温环境会导致中暑、脱水，对老人、儿童和心血管病患者尤为危险。'
                ),
                suggestion='⚠️ 必须立即开启降温设备(空调/风扇)，并保持通风。建议启动强制排风。',
                reference='GB 50736-2012 第3.0.1条: 人员长期停留区域夏季设计温度 24~26°C'
            ))
        elif temp >= self.TEMP_UNCOMFORTABLE:
            insights.append(Insight(
                level='warning',
                title=f'🌡️ 温度偏高: {temp:.1f}°C',
                detail=(
                    f'当前室内温度 {temp:.1f}°C，已超过舒适温度上限 26°C，'
                    f'超出 {temp - self.TEMP_COMFORT_MAX:.1f}°C。'
                    '虽然尚未达到危险水平，但长时间处于该温度下会导致注意力下降、'
                    '心率加快、体感明显不适，婴幼儿和老年人感受会更强烈。'
                ),
                suggestion='💡 建议开启风扇或空调降温。若持续升高，需加强通风。',
                reference='GB 50736-2012: 舒适温度范围 18~26°C'
            ))
        elif temp >= self.TEMP_WARM:
            insights.append(Insight(
                level='info',
                title=f'🌤️ 温度偏暖: {temp:.1f}°C',
                detail=f'当前温度 {temp:.1f}°C，在人体可接受范围内，但略高于最适温度。',
                suggestion='建议保持自然通风，如感觉闷热可开启风扇。',
                reference='GB 50736-2012'
            ))
        elif temp >= self.TEMP_COMFORT_MIN:
            insights.append(Insight(
                level='good',
                title=f'✅ 温度舒适: {temp:.1f}°C',
                detail=(
                    f'当前温度 {temp:.1f}°C，处于 18~26°C 舒适区间内，'
                    '适合人员长期停留，也符合节能要求。'
                ),
                suggestion='无需干预，保持当前状态。',
                reference='GB 50736-2012: 舒适温度范围 18~26°C'
            ))
        elif temp >= self.TEMP_COLD:
            insights.append(Insight(
                level='info',
                title=f'❄️ 温度偏低: {temp:.1f}°C',
                detail=f'当前温度 {temp:.1f}°C，低于舒适温度下限 18°C。',
                suggestion='建议开启暖气或调高空调温度，注意保暖。',
                reference='GB 50736-2012'
            ))
        else:
            insights.append(Insight(
                level='warning',
                title=f'🥶 温度过低: {temp:.1f}°C',
                detail=f'当前温度 {temp:.1f}°C，严重低于舒适范围，可能影响健康。',
                suggestion='⚠️ 必须开启暖气，老人儿童需额外保暖。',
                reference='GB 50736-2012'
            ))

        return insights

    # ============================================
    # 湿度分析
    # ============================================

    def analyze_humidity(self, hum: Optional[float]) -> List[Insight]:
        if hum is None:
            return []
        insights = []

        if hum >= self.HUM_HIGH:
            insights.append(Insight(
                level='warning',
                title=f'💧 湿度过高: {hum:.1f}%',
                detail=(
                    f'当前室内相对湿度 {hum:.1f}%，超过舒适上限 60%。'
                    '根据 GB/T 18883-2022《室内空气质量标准》，'
                    '夏季空调房湿度宜在 40%~80%，冬季采暖房宜在 30%~60%。'
                    '湿度过高易导致墙面发霉、细菌滋生、呼吸道感染，'
                    '对哮喘患者、过敏体质人群影响显著。'
                ),
                suggestion='💡 建议开启除湿机或空调除湿模式。检查是否有漏水点。',
                reference='GB/T 18883-2022: 室内相对湿度 30%~60% (冬季) / 40%~80% (夏季)'
            ))
        elif hum >= self.HUM_COMFORT_MAX:
            insights.append(Insight(
                level='info',
                title=f'💧 湿度略高: {hum:.1f}%',
                detail=f'当前湿度 {hum:.1f}%，处于正常偏湿范围，夏季可接受。',
                suggestion='注意通风，如有不适可开启除湿。',
                reference='GB/T 18883-2022'
            ))
        elif hum >= self.HUM_COMFORT_MIN:
            insights.append(Insight(
                level='good',
                title=f'✅ 湿度适宜: {hum:.1f}%',
                detail=(
                    f'当前湿度 {hum:.1f}%，处于 30%~60% 舒适区间，'
                    '有利于呼吸道健康和皮肤保湿。'
                ),
                suggestion='无需干预，保持当前状态。',
                reference='GB/T 18883-2022'
            ))
        elif hum >= self.HUM_LOW:
            insights.append(Insight(
                level='info',
                title=f'🏜️ 湿度偏低: {hum:.1f}%',
                detail=(
                    f'当前湿度 {hum:.1f}%，低于 30%，空气较为干燥。'
                    '干燥空气会导致皮肤干裂、喉咙不适、静电增多。'
                ),
                suggestion='建议开启加湿器，或在室内放置水盆增加湿度。',
                reference='GB/T 18883-2022'
            ))
        else:
            insights.append(Insight(
                level='warning',
                title=f'🏜️ 极度干燥: {hum:.1f}%',
                detail=f'当前湿度 {hum:.1f}%，极度干燥，对人体非常不利。',
                suggestion='⚠️ 必须开启加湿器，多喝水！',
                reference='GB/T 18883-2022'
            ))

        return insights

    # ============================================
    # 烟雾分析
    # ============================================

    def analyze_smoke(self, smoke: Optional[int]) -> List[Insight]:
        if smoke is None:
            return []
        insights = []

        if smoke >= self.SMOKE_DANGER:
            insights.append(Insight(
                level='danger',
                title=f'🚨 烟雾浓度危险: {smoke}',
                detail=(
                    f'烟雾检测值 {smoke}，已超过 3000 危险阈值。'
                    '极可能存在明火或大量有害气体释放。'
                    '一氧化碳、二氧化硫等燃烧产物可在数分钟内致人昏迷。'
                ),
                suggestion='⚠️ 立即疏散人员！检查火源！拨打 119！自动开启所有通风设备！',
                reference='GB/T 18883-2022: CO 1小时均值 ≤10 mg/m³'
            ))
        elif smoke >= self.SMOKE_HIGH:
            insights.append(Insight(
                level='warning',
                title=f'⚠️ 烟雾浓度偏高: {smoke}',
                detail=(
                    f'烟雾检测值 {smoke}，超出正常范围。'
                    '可能原因为: 厨房油烟扩散、有人吸烟、设备过热冒烟、'
                    '或初期火灾。空气中 PM2.5 和有害气体浓度在上升。'
                ),
                suggestion='💡 建议检查周围环境，确认是否有异常。加强通风换气。',
                reference='GB/T 18883-2022'
            ))
        elif smoke >= self.SMOKE_NORMAL:
            insights.append(Insight(
                level='info',
                title=f'📊 烟雾值略高: {smoke}',
                detail=f'烟雾值 {smoke}，略高于背景水平，仍在安全范围内。',
                suggestion='观察是否有异常气味来源，保持通风。',
                reference='GB/T 18883-2022'
            ))
        else:
            insights.append(Insight(
                level='good',
                title=f'✅ 空气洁净: {smoke}',
                detail='烟雾检测值正常，室内空气中无可燃颗粒物异常。',
                suggestion='无需干预。',
                reference='GB/T 18883-2022'
            ))

        return insights

    # ============================================
    # 综合生成报告
    # ============================================

    def generate_report(self,
                        temp: Optional[float] = None,
                        hum: Optional[float] = None,
                        smoke: Optional[int] = None,
                        fire: bool = False,
                        timestamp: str = ''
                        ) -> AnalysisReport:
        report = AnalysisReport(
            temperature=temp,
            humidity=hum,
            smoke=smoke,
            fire=fire,
            timestamp=timestamp
        )

        # 火灾最高优先级
        if fire:
            report.insights.append(Insight(
                level='danger',
                title='🔥🔥 检测到火焰！',
                detail=(
                    '火焰传感器已触发！这是最高级别的安全警报。'
                    '系统已自动执行: 开锁疏散、蜂鸣器报警、MQTT 紧急上报。'
                    '请立即确认火情真实性并采取灭火措施。'
                ),
                suggestion='🚨 立即确认火情！疏散人员！拨打 119 火警！不要使用电梯！',
                reference='GB 50016-2014 《建筑设计防火规范》'
            ))
            report.summary = '【紧急】检测到火焰！系统已自动解锁并报警，请立即处理！'
            return report

        # 逐项分析
        report.insights.extend(self.analyze_temperature(temp))
        report.insights.extend(self.analyze_humidity(hum))
        report.insights.extend(self.analyze_smoke(smoke))

        # 生成综合摘要
        danger_count = sum(1 for i in report.insights if i.level == 'danger')
        warning_count = sum(1 for i in report.insights if i.level == 'warning')
        if danger_count > 0:
            report.summary = f'检测到 {danger_count} 项危险指标，请立即处理！'
        elif warning_count > 0:
            report.summary = f'检测到 {warning_count} 项需要关注，建议检查相应环境参数。'
        elif report.insights:
            report.summary = '环境参数正常，系统运行良好。'
        else:
            report.summary = '暂无传感器数据。'

        return report


# ============================================
# 单例
# ============================================
engine = AnalysisEngine()