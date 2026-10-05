target_sources(QtRocket_core PRIVATE
    AccelerationData.cpp
    DefaultSimulationOptionFactory.cpp
    EventQueue.cpp
    exception/SimulationCalculationException.cpp
    exception/SimulationException.cpp
    extension/AbstractSimulationExtension.cpp
    extension/AbstractSimulationExtensionProvider.cpp
    FlightData.cpp
    FlightDataBranch.cpp
    FlightDataType.cpp
    FlightDataTypeGroup.cpp
    FlightEvent.cpp
    FlightEventActivation.cpp
    listeners/AbstractSimulationListener.cpp
    SimulationOptions.cpp
    SimulationStepperMethod.cpp
)
