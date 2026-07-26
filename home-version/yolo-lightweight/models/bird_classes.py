#!/usr/bin/env python3
"""
鸟类分类定义模块 — 智能农业生物防控装置

定义害鸟和益鸟的类别信息，包括中文名称、拉丁学名、危害/益处描述、
体型特征、以及推荐的驱离/保护策略。

Author: BioControl Team
Version: 2.0.0
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import List, Optional, Dict, Any


# -----------------------------------------------------------
# 枚举定义
# -----------------------------------------------------------

class BirdCategory(IntEnum):
    """鸟类大类枚举"""
    PEST = 0       # 害鸟 — 需要驱离
    BENEFICIAL = 1  # 益鸟 — 需要保护
    NEUTRAL = 2     # 中性 — 监控但不干预


class ThreatLevel(IntEnum):
    """威胁等级"""
    LOW = 0
    MEDIUM = 1
    HIGH = 2
    CRITICAL = 3


# -----------------------------------------------------------
# 数据类
# -----------------------------------------------------------

@dataclass
class BirdSpecies:
    """单个鸟类物种定义"""
    class_id: int                    # 模型输出类别ID (0-indexed)
    name_en: str                     # 英文俗名
    name_cn: str                     # 中文名称
    scientific_name: str             # 拉丁学名
    category: BirdCategory           # 害鸟/益鸟/中性
    threat_level: ThreatLevel        # 威胁等级 (仅害鸟有效)
    avg_weight_g: float              # 平均体重(g)
    avg_wingspan_cm: float           # 平均翼展(cm)
    flock_size: int                  # 典型群体大小
    description: str                 # 描述
    damage_description: str          # 危害描述 (害鸟)
    benefit_description: str         # 益处描述 (益鸟)
    active_season: List[int]         # 活跃月份 [1-12]
    preferred_crops: List[str]       # 偏好的农作物
    deterrence_methods: List[str]    # 推荐驱离方法
    protection_priority: int = 0     # 保护优先级 (益鸟: 0-10)


# -----------------------------------------------------------
# 鸟类数据库
# -----------------------------------------------------------

BIRD_SPECIES: Dict[int, BirdSpecies] = {
    # ======================== 害鸟 ========================
    0: BirdSpecies(
        class_id=0,
        name_en="Tree Sparrow",
        name_cn="麻雀",
        scientific_name="Passer montanus",
        category=BirdCategory.PEST,
        threat_level=ThreatLevel.HIGH,
        avg_weight_g=24.0,
        avg_wingspan_cm=21.0,
        flock_size=50,
        description="麻雀是农田最常见的小型害鸟，繁殖力极强，"
                    "春季和秋季对农作物造成严重危害。",
        damage_description="啄食稻谷、小麦、高粱、谷子等谷物种子，"
                           "尤其在播种期和成熟期危害严重。单只麻雀每天消耗约4-5g粮食，"
                           "大量集群时可在数日内毁掉整片农田。",
        benefit_description="在育雏期捕食部分昆虫，但整体危害远大于益处。",
        active_season=[3, 4, 5, 6, 7, 8, 9, 10],
        preferred_crops=["稻谷", "小麦", "小米", "高粱", "向日葵"],
        deterrence_methods=["声波驱离", "爆闪灯光", "超声波", "防鸟网", "稻草人"],
    ),
    1: BirdSpecies(
        class_id=1,
        name_en="Carrion Crow",
        name_cn="乌鸦",
        scientific_name="Corvus corone",
        category=BirdCategory.PEST,
        threat_level=ThreatLevel.HIGH,
        avg_weight_g=510.0,
        avg_wingspan_cm=98.0,
        flock_size=20,
        description="乌鸦智商极高，具有社会学习能力，会破坏农作物和果园。",
        damage_description="损害玉米、西瓜、水果等高价值作物，"
                           "会撕开包装袋偷食种子，对播种期危害极大。"
                           "也攻击幼小家禽。",
        benefit_description="食腐，有一定清洁作用；智力高但难以驱离。",
        active_season=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
        preferred_crops=["玉米", "西瓜", "苹果", "花生", "豆类"],
        deterrence_methods=["声波驱离(多种声音轮流)", "爆闪灯", "无人机驱赶", "反射带"],
    ),
    2: BirdSpecies(
        class_id=2,
        name_en="Eurasian Magpie",
        name_cn="喜鹊",
        scientific_name="Pica pica",
        category=BirdCategory.PEST,
        threat_level=ThreatLevel.MEDIUM,
        avg_weight_g=210.0,
        avg_wingspan_cm=56.0,
        flock_size=10,
        description="喜鹊虽然被视为吉祥鸟，但在农业上会偷食种子和果实。",
        damage_description="啄食播下的种子和成熟的水果，"
                           "在果园中啄伤果实导致腐烂。也会偷吃鸡蛋。",
        benefit_description="捕食部分昆虫和鼠类。",
        active_season=[2, 3, 4, 5, 6, 7, 8, 9],
        preferred_crops=["樱桃", "葡萄", "柿子", "花生", "玉米种子"],
        deterrence_methods=["声波驱离", "反光带", "超声波"],
    ),
    3: BirdSpecies(
        class_id=3,
        name_en="Rock Pigeon",
        name_cn="鸽子",
        scientific_name="Columba livia",
        category=BirdCategory.PEST,
        threat_level=ThreatLevel.MEDIUM,
        avg_weight_g=360.0,
        avg_wingspan_cm=68.0,
        flock_size=30,
        description="野鸽在农田和粮仓附近集群觅食，传播多种疾病。",
        damage_description="大量啄食谷物和豆类种子，"
                           "粪便污染粮仓和饲料，传播鸽病、禽流感等。",
        benefit_description="无明显农业益处。",
        active_season=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
        preferred_crops=["小麦", "大麦", "豆类", "花生", "向日葵"],
        deterrence_methods=["超声波", "爆闪灯", "防鸟刺", "声波驱离"],
    ),

    # ======================== 益鸟 ========================
    4: BirdSpecies(
        class_id=4,
        name_en="Barn Swallow",
        name_cn="燕子",
        scientific_name="Hirundo rustica",
        category=BirdCategory.BENEFICIAL,
        threat_level=ThreatLevel.LOW,
        avg_weight_g=19.0,
        avg_wingspan_cm=33.0,
        flock_size=5,
        description="燕子是重要的农业益鸟，以飞行昆虫为食。",
        damage_description="无害。",
        benefit_description="捕食蚊蝇、蚜虫、飞蛾等大量农业害虫，"
                            "一只燕子夏季能捕食数十万只昆虫。燕子多的地方害虫明显减少。",
        active_season=[4, 5, 6, 7, 8, 9],
        preferred_crops=[],
        deterrence_methods=[],
        protection_priority=8,
    ),
    5: BirdSpecies(
        class_id=5,
        name_en="Barn Owl",
        name_cn="猫头鹰",
        scientific_name="Tyto alba",
        category=BirdCategory.BENEFICIAL,
        threat_level=ThreatLevel.LOW,
        avg_weight_g=430.0,
        avg_wingspan_cm=85.0,
        flock_size=1,
        description="猫头鹰是高效的夜间捕食者，主要捕食鼠类。",
        damage_description="无害。",
        benefit_description="夜间捕食田鼠、老鼠等啮齿动物，"
                            "一只猫头鹰每年可捕食3000余只鼠类，"
                            "是生物防治鼠害的重要手段。",
        active_season=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
        preferred_crops=[],
        deterrence_methods=[],
        protection_priority=10,
    ),
    6: BirdSpecies(
        class_id=6,
        name_en="Great Spotted Woodpecker",
        name_cn="啄木鸟",
        scientific_name="Dendrocopos major",
        category=BirdCategory.BENEFICIAL,
        threat_level=ThreatLevel.LOW,
        avg_weight_g=80.0,
        avg_wingspan_cm=40.0,
        flock_size=1,
        description="啄木鸟是森林和果园卫士，啄食树干害虫。",
        damage_description="无害。",
        benefit_description="啄食天牛幼虫、吉丁虫等蛀干害虫，"
                            "保护果树和林木健康。是果园生态的重要维护者。",
        active_season=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
        preferred_crops=[],
        deterrence_methods=[],
        protection_priority=9,
    ),
}


# -----------------------------------------------------------
# 工具函数
# -----------------------------------------------------------

def is_pest(class_id: int) -> bool:
    """判断给定类别ID是否为害鸟。

    Args:
        class_id: 模型输出的类别ID (0-indexed)

    Returns:
        True 如果是害鸟，否则 False
    """
    species = BIRD_SPECIES.get(class_id)
    return species is not None and species.category == BirdCategory.PEST


def is_beneficial(class_id: int) -> bool:
    """判断给定类别ID是否为益鸟。

    Args:
        class_id: 模型输出的类别ID (0-indexed)

    Returns:
        True 如果是益鸟，否则 False
    """
    species = BIRD_SPECIES.get(class_id)
    return species is not None and species.category == BirdCategory.BENEFICIAL


def get_pest_ids() -> List[int]:
    """获取所有害鸟类别ID列表。

    Returns:
        害鸟类别ID的整数列表
    """
    return [bid for bid, sp in BIRD_SPECIES.items()
            if sp.category == BirdCategory.PEST]


def get_beneficial_ids() -> List[int]:
    """获取所有益鸟类别ID列表。

    Returns:
        益鸟类别ID的整数列表
    """
    return [bid for bid, sp in BIRD_SPECIES.items()
            if sp.category == BirdCategory.BENEFICIAL]


def get_all_ids() -> List[int]:
    """获取所有鸟类类别ID列表。

    Returns:
        所有类别ID的整数列表
    """
    return sorted(BIRD_SPECIES.keys())


def get_name_cn(class_id: int) -> str:
    """获取中文名称。

    Args:
        class_id: 类别ID

    Returns:
        中文字符串，如果ID无效则返回 '未知'
    """
    sp = BIRD_SPECIES.get(class_id)
    return sp.name_cn if sp else "未知"


def get_deterrence_methods(class_id: int) -> List[str]:
    """获取针对特定害鸟的驱离方法。

    Args:
        class_id: 类别ID

    Returns:
        驱离方法字符串列表
    """
    sp = BIRD_SPECIES.get(class_id)
    return sp.deterrence_methods if sp else []


def get_active_season(class_id: int) -> List[int]:
    """获取鸟类的活跃月份。

    Args:
        class_id: 类别ID

    Returns:
        活跃月份整数列表 (1-12)
    """
    sp = BIRD_SPECIES.get(class_id)
    return sp.active_season if sp else []


def get_class_names() -> List[str]:
    """获取所有类别名称（按ID排序）。

    Returns:
        按类别ID排序的英文名称列表
    """
    return [BIRD_SPECIES[i].name_en
            for i in sorted(BIRD_SPECIES.keys())]


def get_threat_assessment(class_id: int) -> Dict[str, Any]:
    """获取完整的威胁评估信息。

    Args:
        class_id: 类别ID

    Returns:
        包含威胁信息的字典
    """
    sp = BIRD_SPECIES.get(class_id)
    if not sp:
        return {"error": f"Unknown class_id: {class_id}"}

    return {
        "class_id": sp.class_id,
        "name_cn": sp.name_cn,
        "name_en": sp.name_en,
        "category": sp.category.name,
        "threat_level": sp.threat_level.name if sp.category == BirdCategory.PEST else "N/A",
        "flock_size": sp.flock_size,
        "damage": sp.damage_description if sp.category == BirdCategory.PEST else "N/A",
        "benefit": sp.benefit_description if sp.category == BirdCategory.BENEFICIAL else "N/A",
        "active_months": sp.active_season,
        "deterrence": sp.deterrence_methods,
    }


def evaluate_detection(detections: List[Dict[str, Any]]) -> Dict[str, Any]:
    """评估一批检测结果的威胁程度并给出行动建议。

    Args:
        detections: 检测结果列表，每项至少包含 'class_id' 和 'confidence'

    Returns:
        包含评估结果和行动建议的字典
    """
    pest_count = 0
    beneficial_count = 0
    pest_details: List[Dict] = []

    for det in detections:
        cid = det.get("class_id", -1)
        conf = det.get("confidence", 0.0)
        sp = BIRD_SPECIES.get(cid)
        if sp is None:
            continue
        if sp.category == BirdCategory.PEST:
            pest_count += 1
            pest_details.append({
                "name": sp.name_cn,
                "threat": sp.threat_level.name,
                "confidence": round(conf, 3),
                "action": sp.deterrence_methods[0] if sp.deterrence_methods else "手动干预",
            })
        elif sp.category == BirdCategory.BENEFICIAL:
            beneficial_count += 1

    # 决策逻辑
    if pest_count == 0:
        action = "none"
        action_desc = "无需驱离，当前无威胁"
    elif pest_count < 3:
        action = "deter"
        action_desc = f"轻度威胁({pest_count}只害鸟)，启动声光驱离"
    elif pest_count < 10:
        action = "deter_strong"
        action_desc = f"中度威胁({pest_count}只害鸟)，启动强力驱离模式"
    else:
        action = "deter_urgent"
        action_desc = f"严重威胁({pest_count}只害鸟)，立即启动紧急驱离!"

    return {
        "pest_count": pest_count,
        "beneficial_count": beneficial_count,
        "action": action,
        "action_description": action_desc,
        "pest_details": pest_details,
        "beneficial_present": beneficial_count > 0,
    }


# -----------------------------------------------------------
# 模块自检
# -----------------------------------------------------------
if __name__ == "__main__":
    print("=" * 60)
    print("鸟类分类数据库 — 模块自检")
    print("=" * 60)

    print(f"\n总类别数: {len(BIRD_SPECIES)}")
    print(f"害鸟: {get_pest_ids()} ({len(get_pest_ids())}种)")
    print(f"益鸟: {get_beneficial_ids()} ({len(get_beneficial_ids())}种)")

    # 模拟检测结果
    mock_detections = [
        {"class_id": 0, "confidence": 0.92},  # 麻雀
        {"class_id": 0, "confidence": 0.87},  # 麻雀
        {"class_id": 1, "confidence": 0.78},  # 乌鸦
        {"class_id": 5, "confidence": 0.95},  # 猫头鹰(益鸟)
    ]

    print("\n模拟检测评估:")
    result = evaluate_detection(mock_detections)
    for k, v in result.items():
        print(f"  {k}: {v}")

    print("\n各类别详细信息:")
    for cid, sp in BIRD_SPECIES.items():
        print(f"\n  [{cid}] {sp.name_cn} ({sp.name_en})")
        print(f"      类别: {sp.category.name} | 威胁: {sp.threat_level.name}")
        print(f"      体重: {sp.avg_weight_g}g | 翼展: {sp.avg_wingspan_cm}cm")
        print(f"      群体: {sp.flock_size}只 | 活跃月: {sp.active_season}")

    print("\n✅ 鸟类分类模块自检通过")
