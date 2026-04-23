from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Any


@dataclass(frozen=True)
class CandidateBlendRecipe:
    recipe_id: str
    display_name: str
    electro_blend: float
    thermal_blend: float
    description: str


@dataclass(frozen=True)
class CalibrationSearchPlan:
    plan_id: str
    display_name: str
    description: str
    anchor_dataset_limit: int
    include_single_anchor_candidates: bool
    include_weighted_family_blend_candidate: bool
    rc_branch_count_options: tuple[int, ...]
    blend_recipes: tuple[CandidateBlendRecipe, ...]
    screening_dataset_limit: int
    max_full_validation_candidates: int
    max_candidates: int
    ordering_notes: tuple[str, ...] = ()
    pruning_notes: tuple[str, ...] = ()


_FAST_PRODUCT_DEFAULT = CalibrationSearchPlan(
    plan_id="fast_product_default",
    display_name="Fast Product Default",
    description=(
        "Commercially relevant default search: only 1RC/2RC structures, top weighted room anchors, "
        "and two pragmatic blend recipes."
    ),
    anchor_dataset_limit=2,
    include_single_anchor_candidates=True,
    include_weighted_family_blend_candidate=True,
    rc_branch_count_options=(1, 2),
    blend_recipes=(
        CandidateBlendRecipe(
            recipe_id="electrical_commit",
            display_name="Electrical Commit",
            electro_blend=1.0,
            thermal_blend=0.0,
            description="Favor direct electrical adoption with no extra thermal push.",
        ),
        CandidateBlendRecipe(
            recipe_id="guarded_thermal",
            display_name="Guarded Thermal",
            electro_blend=0.65,
            thermal_blend=0.2,
            description="Keep most of the electrical fit while allowing light thermal adjustment.",
        ),
    ),
    screening_dataset_limit=2,
    max_full_validation_candidates=6,
    max_candidates=12,
    ordering_notes=(
        "Sort anchors by weighted room-envelope pain first.",
        "Keep deterministic rc count and recipe ordering.",
    ),
    pruning_notes=(
        "Drop 0RC candidates for the product-default path.",
        "Avoid broad electro/thermal cartesian sweeps.",
        "Cap full-manifest validation to the strongest screened candidates.",
    ),
)


_BALANCED_DEFAULT = CalibrationSearchPlan(
    plan_id="balanced_default",
    display_name="Balanced Default",
    description=(
        "Slightly broader search for electro-thermal compromise studies without opening the full debug grid."
    ),
    anchor_dataset_limit=2,
    include_single_anchor_candidates=True,
    include_weighted_family_blend_candidate=True,
    rc_branch_count_options=(1, 2),
    blend_recipes=(
        CandidateBlendRecipe(
            recipe_id="balanced_guardrail",
            display_name="Balanced Guardrail",
            electro_blend=0.65,
            thermal_blend=0.2,
            description="Balanced starting point with mild thermal blending.",
        ),
        CandidateBlendRecipe(
            recipe_id="thermal_push",
            display_name="Thermal Push",
            electro_blend=0.65,
            thermal_blend=0.6,
            description="Allow stronger thermal blending while keeping meaningful electrical carry-through.",
        ),
        CandidateBlendRecipe(
            recipe_id="full_commit",
            display_name="Full Commit",
            electro_blend=1.0,
            thermal_blend=0.6,
            description="High electrical commitment plus stronger thermal adoption for comparison.",
        ),
    ),
    screening_dataset_limit=3,
    max_full_validation_candidates=8,
    max_candidates=18,
    ordering_notes=(
        "Evaluate the highest-weighted room anchors first.",
        "Keep one weighted family-blend candidate for each structure/recipe pair.",
    ),
    pruning_notes=(
        "Still avoid exhaustive cartesian search.",
        "Keep anchor count limited to the strongest weighted representatives.",
    ),
)


_EXHAUSTIVE_DEBUG = CalibrationSearchPlan(
    plan_id="exhaustive_debug",
    display_name="Exhaustive Debug",
    description=(
        "Broader offline tuning plan for deeper investigation. Still deterministic, but materially larger."
    ),
    anchor_dataset_limit=4,
    include_single_anchor_candidates=True,
    include_weighted_family_blend_candidate=True,
    rc_branch_count_options=(0, 1, 2),
    blend_recipes=(
        CandidateBlendRecipe(
            recipe_id="light_electro",
            display_name="Light Electro",
            electro_blend=0.35,
            thermal_blend=0.0,
            description="Small electrical nudge without thermal blending.",
        ),
        CandidateBlendRecipe(
            recipe_id="light_guarded",
            display_name="Light Guarded",
            electro_blend=0.35,
            thermal_blend=0.2,
            description="Small electrical nudge with light thermal blending.",
        ),
        CandidateBlendRecipe(
            recipe_id="mid_electro",
            display_name="Mid Electro",
            electro_blend=0.65,
            thermal_blend=0.0,
            description="Mid electrical adoption without thermal blending.",
        ),
        CandidateBlendRecipe(
            recipe_id="mid_guarded",
            display_name="Mid Guarded",
            electro_blend=0.65,
            thermal_blend=0.2,
            description="Mid electrical adoption with light thermal blending.",
        ),
        CandidateBlendRecipe(
            recipe_id="mid_thermal_push",
            display_name="Mid Thermal Push",
            electro_blend=0.65,
            thermal_blend=0.6,
            description="Mid electrical adoption with stronger thermal blending.",
        ),
        CandidateBlendRecipe(
            recipe_id="full_electro",
            display_name="Full Electro",
            electro_blend=1.0,
            thermal_blend=0.0,
            description="Full electrical adoption without thermal blending.",
        ),
        CandidateBlendRecipe(
            recipe_id="full_guarded",
            display_name="Full Guarded",
            electro_blend=1.0,
            thermal_blend=0.2,
            description="Full electrical adoption with light thermal blending.",
        ),
        CandidateBlendRecipe(
            recipe_id="full_thermal_push",
            display_name="Full Thermal Push",
            electro_blend=1.0,
            thermal_blend=0.6,
            description="Full electrical adoption with stronger thermal blending.",
        ),
    ),
    screening_dataset_limit=4,
    max_full_validation_candidates=16,
    max_candidates=120,
    ordering_notes=(
        "Retain deterministic anchor/rc/recipe ordering for repeatability.",
    ),
    pruning_notes=(
        "Still apply sanity filters and screen before full-manifest validation.",
    ),
)


DEFAULT_SEARCH_PLAN_ID_BY_PROFILE = {
    "electrical_first": _FAST_PRODUCT_DEFAULT.plan_id,
    "balanced_electro_thermal": _BALANCED_DEFAULT.plan_id,
}


_SEARCH_PLANS: dict[str, CalibrationSearchPlan] = {
    _FAST_PRODUCT_DEFAULT.plan_id: _FAST_PRODUCT_DEFAULT,
    _BALANCED_DEFAULT.plan_id: _BALANCED_DEFAULT,
    _EXHAUSTIVE_DEBUG.plan_id: _EXHAUSTIVE_DEBUG,
}


def list_calibration_search_plans() -> list[CalibrationSearchPlan]:
    return list(_SEARCH_PLANS.values())


def get_default_search_plan_id(calibration_profile_id: str) -> str:
    return DEFAULT_SEARCH_PLAN_ID_BY_PROFILE.get(str(calibration_profile_id).strip(), _FAST_PRODUCT_DEFAULT.plan_id)


def get_calibration_search_plan(plan_id: str | None = None, *, calibration_profile_id: str | None = None) -> CalibrationSearchPlan:
    resolved_id = (
        str(plan_id).strip()
        if plan_id not in (None, "")
        else get_default_search_plan_id(str(calibration_profile_id or "").strip())
    )
    try:
        return _SEARCH_PLANS[resolved_id]
    except KeyError as exc:
        raise ValueError(
            f"Unknown calibration search plan '{resolved_id}'. "
            f"Available plans: {', '.join(sorted(_SEARCH_PLANS)) or 'none'}."
        ) from exc


def calibration_search_plan_to_dict(plan: CalibrationSearchPlan) -> dict[str, Any]:
    return asdict(plan)
