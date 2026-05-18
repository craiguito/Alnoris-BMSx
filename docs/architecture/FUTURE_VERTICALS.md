# Future Verticals

TwinCore is intended to support multiple engineering verticals. BatteryTwin is active today. The other verticals listed here are planned definitions only and do not have runtime packages yet.

## FusionTwin

FusionTwin is planned for fusion energy systems.

Planned components include:

- vacuum vessel
- first wall panel
- divertor tile
- blanket module
- magnet coil
- coolant loop
- tritium system
- diagnostic sensor

Planned scenario and solver areas include heat flux, neutron loading, disruption loading, coolant network behavior, tritium inventory, structural stress, material damage, and OpenMC adapter workflows.

TwinCore reuse:

- asset graphs for machine and subsystem hierarchy
- component twins for replaceable hardware
- geometry references for generated or external layouts
- provenance and credibility records for solver traceability

## FissionTwin

FissionTwin is planned for advanced nuclear fission systems.

Planned components include:

- reactor vessel
- core
- fuel assembly
- fuel pin
- control rod
- coolant channel
- heat exchanger
- shielding
- sensor

Planned scenario and solver areas include normal operation, decay heat, loss of flow, power ramps, fuel health, neutronics runs, OpenMC adapters, and MOOSE adapters.

TwinCore reuse:

- scenario and run manifests
- validation packs and uncertainty classifications
- report records for screening summaries
- common project and asset persistence

## GridTwin

GridTwin is planned for power grid and microgrid systems.

Planned components include:

- substation
- transformer
- feeder
- line
- breaker
- relay
- inverter
- load
- BESS
- solar plant
- wind plant
- EV charging site

Planned scenario and solver areas include power-flow screening, battery dispatch, outages, islanded microgrids, renewable curtailment, data center load growth, OpenDSS adapters, and pandapower adapters.

TwinCore reuse:

- template-driven system creation
- project-scoped asset IDs
- run history and report summaries
- adapter and solver capability registration

## AeroTwin

AeroTwin is planned for aerospace, propulsion, and mission systems.

Planned components include:

- aircraft
- airframe
- wing
- fuselage
- propulsion unit
- motor
- propeller
- battery system
- thermal loop
- actuator
- avionics
- sensor

Planned scenario and solver areas include mission profiles, hover, climb, cruise, propulsion failure, thermal stress, gust loads, landing impact, SU2 adapters, and BatteryTwin adapter workflows.

TwinCore reuse:

- cross-vertical battery system links
- mission scenario records
- solver plugin and adapter contracts
- credibility cards for screening-level analyses

## What Not To Build Yet

Do not build full future vertical physics until BatteryTwin service contracts and solver migration are stable.

Do not create placeholder packages that imply implemented products. The current future verticals are registry definitions, documentation, and testable architecture contracts only.
