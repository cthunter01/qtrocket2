target_sources(QtRocket_core PRIVATE
    AbstractAerodynamicCalculator.cpp
    AerodynamicForces.cpp
    barrowman/ComponentAssemblyCalc.cpp
    barrowman/Naca1307FinBodyInterference.cpp
    barrowman/RocketComponentCalc.cpp
    FlightConditions.cpp
    ForceMap.cpp
    lookup/CsvMachAoALookup.cpp
    lookup/MachAoALookup.cpp
    LookupTableDragCalculator.cpp
    LookupTableStabilityCalculator.cpp
)
