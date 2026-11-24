# GUI area 11, simulation: the "Flight simulations" tab (OpenRocket's SimulationPanel): its table's columns, model,
# view and delegate, the column chooser, the operations on simulations and the panel (Milestone 4, plan 11.2). It
# comes after flightconfig and before main; the dialogs it opens (the simulation settings, the run and the plot
# dialogs) are in dialogs/simulation/ and dialogs/plot/, which come before it and never include it. Not the core's
# simulation/ (QtRocket/simulation/...), which every area may include.
# Each source with its header (${gui_include_dir}/simulation/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
)
