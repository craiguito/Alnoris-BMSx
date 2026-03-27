from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class ReferenceCell:
    name: str
    chemistry: str
    cell_nominal_voltage: float
    cell_full_voltage: float
    cell_empty_voltage: float
    cell_cutoff_voltage: float
    cell_capacity_ah: float
    internal_resistance_ohm_per_cell: float
    recommended_discharge_current_a: float
    pack_mass_kg: float
    pack_heat_capacity_j_per_kgk: float
    cooling_coeff_w_per_k: float


REFERENCE_CELLS: dict[str, ReferenceCell] = {
    "panasonic_ncr18650b": ReferenceCell(
        name="Panasonic NCR18650B",
        chemistry="NCA energy 18650",
        cell_nominal_voltage=3.6,
        cell_full_voltage=4.2,
        cell_empty_voltage=3.0,
        cell_cutoff_voltage=3.0,
        cell_capacity_ah=3.35,
        internal_resistance_ohm_per_cell=0.035,
        recommended_discharge_current_a=1.675,
        pack_mass_kg=0.048,
        pack_heat_capacity_j_per_kgk=900.0,
        cooling_coeff_w_per_k=0.35,
    ),
    "samsung_30q": ReferenceCell(
        name="Samsung INR18650-30Q",
        chemistry="NMC power 18650",
        cell_nominal_voltage=3.6,
        cell_full_voltage=4.2,
        cell_empty_voltage=2.5,
        cell_cutoff_voltage=2.5,
        cell_capacity_ah=3.0,
        internal_resistance_ohm_per_cell=0.02,
        recommended_discharge_current_a=3.0,
        pack_mass_kg=0.046,
        pack_heat_capacity_j_per_kgk=900.0,
        cooling_coeff_w_per_k=0.45,
    ),
    "a123_anr26650m1b": ReferenceCell(
        name="A123 ANR26650M1-B",
        chemistry="LFP high-power 26650",
        cell_nominal_voltage=3.3,
        cell_full_voltage=3.6,
        cell_empty_voltage=2.0,
        cell_cutoff_voltage=2.0,
        cell_capacity_ah=2.5,
        internal_resistance_ohm_per_cell=0.006,
        recommended_discharge_current_a=7.5,
        pack_mass_kg=0.076,
        pack_heat_capacity_j_per_kgk=950.0,
        cooling_coeff_w_per_k=0.55,
    ),
}
