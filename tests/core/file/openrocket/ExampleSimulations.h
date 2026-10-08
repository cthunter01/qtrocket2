#pragma once

// What OpenRocket makes of the simulations of its sixteen example designs (data/examples), for
// the test that loads their <simulations> elements. Test-only.
//
// Written by gen_examples.py of run 9b, part S3, from the output of the tier 9 scout's Java
// probe ExamplesProbe, which loaded every example with OpenRocket's own loader and printed its
// simulations. A line is the probe's, without what depends on the components of the rocket
// (the test's rocket has the flight configurations of the file and none of its components):
// the names of a warning's sources, the Java class of an extension, and of a branch the number
// of events whose source is not in the rocket and the description of a type that is not built
// in. A character outside the printable ASCII range is written "<U+XXXX>".
//
// The lines of a simulation:
// - "sim[<n>] '<name>' status=<Simulation.getStatus() after the load> fcid=<the first eight
//   characters of the configuration id> hasSimData=... hasSummary=... seedFixed=...";
// - "  ext id=<id> name=<name>" for every extension;
// - "  summary ...": the ten summary values, computed from the first branch;
// - "  warning <class> prio=<priority> id=<id> desc='<text without sources>'" for every
//   warning of the flight data;
// - "  branch[<n>] '<name>' rows=... types=<columns> (builtin <n>, other <n>) srcId=...
//   optAlt=... tOptAlt=... optDelay=... sepTime=... events={<type>=<count>, ...}
//   withSource=<events with a source> withData=<events with data> mutable=false".

#include <array>
#include <span>
#include <string_view>

namespace QtRocket::Test
{

/// One example design and what its simulations load as.
struct ExampleSimulations
{
    /// The file in data/examples.
    std::string_view file;
    /// The lines of its simulations (see above).
    std::span<const std::string_view> lines;
    /// The number of simulations, and over all of them the branches, the rows of the branches,
    /// the events and the warnings of the stored flight data.
    int simulations;
    int branches;
    int rows;
    int events;
    int warnings;
};

/// 3D printable nose cone and fins.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 16> kExampleSimulationLines00{
    "sim[0] 'Simulation 1' status=LOADED fcid=da326836 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=38.733 maxVel=24.764 maxAcc=124.056 maxMach=0.073 tApogee=3.085 "
    "tFlight=12.194 vGround=4.691 vRod=13.962 vDeploy=5.886 optDelay=2.3559535841670214",
    "  branch[0] 'Sustainer' rows=161 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=38.732509659969075 tOptAlt=3.0859535841670214 optDelay=2.3559535841670214 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, TUMBLE=1, APOGEE=1, "
    "EJECTION_CHARGE=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=05896b5a hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=112.011 maxVel=48.501 maxAcc=152.721 maxMach=0.143 tApogee=4.912 "
    "tFlight=31.135 vGround=4.696 vRod=16.127 vDeploy=1.404 optDelay=4.055746822351933",
    "  branch[0] 'Sustainer' rows=298 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=112.01237194831236 tOptAlt=4.915746822351934 optDelay=4.055746822351933 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, TUMBLE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
    "sim[2] 'Simulation 3 - too short delay' status=LOADED fcid=b13e8ca2 hasSimData=true "
    "hasSummary=true seedFixed=false",
    "  summary maxAlt=243.099 maxVel=83.142 maxAcc=167.793 maxMach=0.245 tApogee=5.431 "
    "tFlight=61.931 vGround=4.814 vRod=16.301 vDeploy=26.964000000000006 "
    "optDelay=5.5645357799959605",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=22700265-0a82-46d3-beeb-e566a5687070 "
    "desc='Recovery device deployment at high speed (26.9 m/s)'",
    "  branch[0] 'Sustainer' rows=487 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=272.7645712520347 tOptAlt=7.424535779995961 optDelay=5.5645357799959605 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "SIM_WARN=1, RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=6 withData=1 mutable=false",
    "sim[3] 'Simulation 4' status=LOADED fcid=e1dc7488 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=271.985 maxVel=83.16 maxAcc=167.807 maxMach=0.245 tApogee=7.184 "
    "tFlight=70.339 vGround=4.76 vRod=16.302 vDeploy=5.968333333333338 optDelay=5.569963470526746",
    "  branch[0] 'Sustainer' rows=528 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=272.7810587742362 tOptAlt=7.429963470526746 optDelay=5.569963470526746 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
    "sim[4] 'Simulation 5' status=LOADED fcid=5d9a2765 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=272.559 maxVel=83.143 maxAcc=167.808 maxMach=0.245 tApogee=7.443 "
    "tFlight=69.76 vGround=4.665 vRod=16.302 vDeploy=10.228 optDelay=5.582773978083612",
    "  branch[0] 'Sustainer' rows=515 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=272.55913044492615 tOptAlt=7.442773978083612 optDelay=5.582773978083612 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, TUMBLE=1, APOGEE=1, "
    "EJECTION_CHARGE=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// A simple model rocket.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 16> kExampleSimulationLines01{
    "sim[0] 'Simulation 1' status=LOADED fcid=da326836 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=50.59 maxVel=29.249 maxAcc=143.649 maxMach=0.086 tApogee=3.481 "
    "tFlight=15.888 vGround=4.681 vRod=15.365 vDeploy=2.644333333333332 "
    "optDelay=2.7508833983690186",
    "  branch[0] 'Sustainer' rows=233 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=50.58976299554823 tOptAlt=3.4808833983690186 optDelay=2.7508833983690186 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=05896b5a hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=135.071 maxVel=53.178 maxAcc=182.862 maxMach=0.157 tApogee=5.265 "
    "tFlight=37.935 vGround=4.709 vRod=17.788 vDeploy=4.002000000000004 "
    "optDelay=4.3731445312499995",
    "  branch[0] 'Sustainer' rows=399 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=135.30664245086587 tOptAlt=5.40314453125 optDelay=4.3731445312499995 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
    "sim[2] 'Simulation 3 - too short delay' status=LOADED fcid=b13e8ca2 hasSimData=true "
    "hasSummary=true seedFixed=false",
    "  summary maxAlt=278.656 maxVel=95.284 maxAcc=191.326 maxMach=0.281 tApogee=5.419 "
    "tFlight=72.605 vGround=4.661 vRod=17.794 vDeploy=31.96933333333334 optDelay=5.985295505809697",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=9e26b796-6349-4902-ba33-14816cf9d3bd "
    "desc='Recovery device deployment at high speed (31.9 m/s)'",
    "  branch[0] 'Sustainer' rows=598 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=320.00735504489097 tOptAlt=7.845295505809697 optDelay=5.985295505809697 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "SIM_WARN=1, RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=6 withData=1 mutable=false",
    "sim[3] 'Simulation 4' status=LOADED fcid=e1dc7488 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=316.583 maxVel=95.282 maxAcc=191.302 maxMach=0.281 tApogee=7.261 "
    "tFlight=83.514 vGround=4.73 vRod=17.793 vDeploy=9.924000000000005 optDelay=5.964397075605982",
    "  branch[0] 'Sustainer' rows=655 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=319.72844690784115 tOptAlt=7.824397075605982 optDelay=5.964397075605982 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
    "sim[4] 'Simulation 5' status=LOADED fcid=5d9a2765 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=319.75 maxVel=95.278 maxAcc=191.316 maxMach=0.281 tApogee=7.806 "
    "tFlight=84.386 vGround=4.574 vRod=17.794 vDeploy=9.37200000000001 optDelay=5.994559749653255",
    "  branch[0] 'Sustainer' rows=695 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=319.749685449882 tOptAlt=7.854559749653255 optDelay=5.994559749653255 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// ARC payload rocket.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 4> kExampleSimulationLines02{
    "sim[0] 'Simulation 1' status=LOADED fcid=c2a3649d hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=451.359 maxVel=150.261 maxAcc=196.76 maxMach=0.442 tApogee=8.172 "
    "tFlight=109.981 vGround=4.642 vRod=19.691 vDeploy=16.041999999999998 "
    "optDelay=6.792161080134241",
    "  branch[0] 'Payload' rows=711 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=451.3585089515802 tOptAlt=8.22216108013424 optDelay=6.792161080134241 sepTime=10.43 "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=7 withData=0 mutable=false",
    "  branch[1] 'Booster stage' rows=702 types=58 (builtin 58, other 0) srcId=null optAlt=NaN "
    "tOptAlt=NaN optDelay=NaN sepTime=10.43 events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=5 withData=0 mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Airstart timing.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 20> kExampleSimulationLines03{
    "sim[0] 'Simulation 1' status=LOADED fcid=99b8d6a6 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=1316.026 maxVel=186.296 maxAcc=119.185 maxMach=0.549 tApogee=15.606 "
    "tFlight=94.532 vGround=5.362 vRod=15.187 vDeploy=27.186 optDelay=12.300000000000093",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=440e8d83-eb39-4595-8b42-57c790f41c7f "
    "desc='Recovery device deployment at high speed (27.2 m/s)'",
    "  branch[0] 'Sustainer' rows=655 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=1316.0264699153815 tOptAlt=15.656000000000093 optDelay=12.300000000000093 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=8 "
    "withData=1 mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=bc7de2d0 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=1289.023 maxVel=190.063 maxAcc=125.637 maxMach=0.56 tApogee=16.004 "
    "tFlight=92.573 vGround=5.428 vRod=9.16 vDeploy=27.186 optDelay=12.648000000000097",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=df4652f2-e218-4fa7-baae-1141fe110362 "
    "desc='Recovery device deployment at high speed (27.2 m/s)'",
    "  branch[0] 'Sustainer' rows=650 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=1289.0225537483936 tOptAlt=16.004000000000097 optDelay=12.648000000000097 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=8 "
    "withData=1 mutable=false",
    "sim[2] 'Simulation 3' status=LOADED fcid=bbf39580 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=1290.97 maxVel=177.545 maxAcc=94.145 maxMach=0.524 tApogee=16.542 "
    "tFlight=93.909 vGround=5.356 vRod=9.16 vDeploy=27.199 optDelay=12.21750000000012",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=b035538e-a962-4118-8a67-3ecd29b07123 "
    "desc='Recovery device deployment at high speed (27.2 m/s)'",
    "  branch[0] 'Sustainer' rows=640 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=1290.9704202059377 tOptAlt=16.54150000000012 optDelay=12.21750000000012 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=8 "
    "withData=1 mutable=false",
    "sim[3] 'Simulation 4' status=LOADED fcid=37647ba6 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=1297.763 maxVel=155.681 maxAcc=61.523 maxMach=0.46 tApogee=17.71 "
    "tFlight=94.091 vGround=5.514 vRod=9.16 vDeploy=27.183 optDelay=11.436000000000144",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=ede9e93b-733d-4606-a7b0-8ddbeb94d38e "
    "desc='Recovery device deployment at high speed (27.2 m/s)'",
    "  branch[0] 'Sustainer' rows=650 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=1297.7629600432842 tOptAlt=17.760000000000144 optDelay=11.436000000000144 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=8 "
    "withData=1 mutable=false",
    "sim[4] 'Simulation 5' status=LOADED fcid=7c4d659a hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=1265.477 maxVel=136.5 maxAcc=63.822 maxMach=0.404 tApogee=18.86 "
    "tFlight=93.936 vGround=5.275 vRod=9.16 vDeploy=27.177 optDelay=10.586000000000153",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=35366b04-de2c-4917-9fd5-a87fe94f398e "
    "desc='Recovery device deployment at high speed (27.2 m/s)'",
    "  branch[0] 'Sustainer' rows=670 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=1265.4766813038907 tOptAlt=18.910000000000153 optDelay=10.586000000000153 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=8 "
    "withData=1 mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Chute release.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 6> kExampleSimulationLines04{
    "sim[0] 'Simulation 2' status=LOADED fcid=95aac292 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=307.726 maxVel=72.071 maxAcc=62.139 maxMach=0.212 tApogee=8.424 "
    "tFlight=51.822 vGround=5.309 vRod=18.492 vDeploy=14.383 optDelay=6.174250544008369",
    "  branch[0] 'Sustainer' rows=439 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=307.72567915814074 tOptAlt=8.474250544008369 optDelay=6.174250544008369 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=7 withData=0 "
    "mutable=false",
    "sim[1] 'Simulation 3' status=LOADED fcid=3c6373fb hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=487.936 maxVel=106.285 maxAcc=88.677 maxMach=0.313 tApogee=9.962 "
    "tFlight=64.897 vGround=5.097 vRod=22.705 vDeploy=14.343 optDelay=8.153964376497436",
    "  branch[0] 'Sustainer' rows=575 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=487.93627134701677 tOptAlt=9.961964376497436 optDelay=8.153964376497436 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=7 withData=0 "
    "mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Clustered motors.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 16> kExampleSimulationLines05{
    "sim[0] 'Simulation 1' status=LOADED fcid=da326836 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=57.857 maxVel=32.36 maxAcc=158.48 maxMach=0.095 tApogee=3.622 tFlight=13.745 "
    "vGround=6.27 vRod=16.358 vDeploy=0.8999999999999988 optDelay=2.9421980849224174",
    "  branch[0] 'Sustainer' rows=153 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=57.856723856343656 tOptAlt=3.6721980849224174 optDelay=2.9421980849224174 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=05896b5a hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=142.209 maxVel=57.628 maxAcc=200.808 maxMach=0.17 tApogee=5.316 "
    "tFlight=29.569 vGround=6.197 vRod=19.178 vDeploy=3.428000000000005 optDelay=4.32314453125",
    "  branch[0] 'Sustainer' rows=241 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=142.3324577534434 tOptAlt=5.35314453125 optDelay=4.32314453125 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
    "sim[2] 'Simulation 3 - too short delay' status=LOADED fcid=b13e8ca2 hasSimData=true "
    "hasSummary=true seedFixed=false",
    "  summary maxAlt=279.136 maxVel=98.494 maxAcc=208.827 maxMach=0.29 tApogee=5.637 "
    "tFlight=52.959 vGround=6.207 vRod=18.601 vDeploy=27.55133333333334 optDelay=5.573144531249992",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=76a35ffa-fd95-40a7-9c8e-bcc900bbdb48 "
    "desc='Recovery device deployment at high speed (27.5 m/s)'",
    "  branch[0] 'Sustainer' rows=337 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=307.48948488047273 tOptAlt=7.433144531249992 optDelay=5.573144531249992 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "SIM_WARN=1, RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=6 withData=1 mutable=false",
    "sim[3] 'Simulation 4' status=LOADED fcid=e1dc7488 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=306.964 maxVel=98.493 maxAcc=208.813 maxMach=0.29 tApogee=7.277 "
    "tFlight=59.255 vGround=6.188 vRod=18.6 vDeploy=5.634000000000005 optDelay=5.573144531249999",
    "  branch[0] 'Sustainer' rows=376 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=307.52561590003495 tOptAlt=7.433144531249999 optDelay=5.573144531249999 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
    "sim[4] 'Simulation 5' status=LOADED fcid=5d9a2765 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=307.265 maxVel=98.465 maxAcc=208.81 maxMach=0.29 tApogee=7.41 tFlight=58.813 "
    "vGround=6.257 vRod=18.6 vDeploy=11.953500000000009 optDelay=5.599601909843206",
    "  branch[0] 'Sustainer' rows=405 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=307.2653503424937 tOptAlt=7.459601909843206 optDelay=5.599601909843206 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Deployable payload.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 22> kExampleSimulationLines06{
    "sim[0] 'Simulation 1' status=LOADED fcid=da326836 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=31.783 maxVel=21.802 maxAcc=110.942 maxMach=0.065 tApogee=2.867 "
    "tFlight=10.759 vGround=4.398 vRod=13.003 vDeploy=7.840666666666666 "
    "optDelay=2.1373946365259506",
    "  branch[0] 'Deployable Payload' rows=187 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=31.782738904625507 tOptAlt=2.8673946365259506 optDelay=2.1373946365259506 sepTime=3.73 "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=7 withData=0 mutable=false",
    "  branch[1] 'Booster' rows=176 types=58 (builtin 58, other 0) srcId=null optAlt=NaN "
    "tOptAlt=NaN optDelay=NaN sepTime=3.73 events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=5 withData=0 mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=05896b5a hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=95.512 maxVel=40.945 maxAcc=143.134 maxMach=0.121 tApogee=4.729 "
    "tFlight=29.163 vGround=4.382 vRod=15.19 vDeploy=3.5599999999999965 "
    "optDelay=3.6987948423463903",
    "  branch[0] 'Deployable Payload' rows=325 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=95.51168145404225 tOptAlt=4.728794842346391 optDelay=3.6987948423463903 sepTime=5.03 "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=7 withData=0 mutable=false",
    "  branch[1] 'Booster' rows=282 types=58 (builtin 58, other 0) srcId=null optAlt=NaN "
    "tOptAlt=NaN optDelay=NaN sepTime=5.03 events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=5 withData=0 mutable=false",
    "sim[2] 'Simulation 3 - too short delay' status=LOADED fcid=b13e8ca2 hasSimData=true "
    "hasSummary=true seedFixed=false",
    "  summary maxAlt=231.182 maxVel=77.292 maxAcc=151.733 maxMach=0.228 tApogee=5.382 "
    "tFlight=63.874 vGround=4.559 vRod=15.444 vDeploy=27.91533333333334 optDelay=4.839785922137489",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=0df5738a-a9e9-4bf9-995b-2d2cb8647ecf "
    "desc='Recovery device deployment at high speed (27.9 m/s)'",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=41b1f141-6c56-4083-b540-75fb0ef3f87d "
    "desc='Recovery device deployment at high speed (27.9 m/s)'",
    "  branch[0] 'Deployable Payload' rows=549 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=250.51914997638997 tOptAlt=6.699785922137489 optDelay=4.839785922137489 sepTime=4.86 "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, SIM_WARN=1, RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, "
    "SIMULATION_END=1} withSource=7 withData=1 mutable=false",
    "  branch[1] 'Booster' rows=457 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=250.51914997638997 tOptAlt=6.699785922137489 optDelay=4.839785922137489 sepTime=4.86 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, SIM_WARN=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=1 mutable=false",
    "sim[3] 'Simulation 4' status=LOADED fcid=e1dc7488 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=262.499 maxVel=77.285 maxAcc=151.737 maxMach=0.228 tApogee=7.182 "
    "tFlight=73.53 vGround=4.358 vRod=15.444 vDeploy=7.478666666666671 optDelay=5.627986783873166",
    "  branch[0] 'Deployable Payload' rows=583 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=263.7315132561721 tOptAlt=7.487986783873167 optDelay=5.627986783873166 sepTime=6.86 "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=7 withData=0 mutable=false",
    "  branch[1] 'Booster' rows=475 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=263.7315132561721 tOptAlt=7.487986783873167 optDelay=5.627986783873166 sepTime=6.86 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=0 mutable=false",
    "sim[4] 'Simulation 5' status=LOADED fcid=5d9a2765 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=263.275 maxVel=77.271 maxAcc=151.731 maxMach=0.228 tApogee=7.516 "
    "tFlight=72.812 vGround=4.399 vRod=15.443 vDeploy=13.29000000000001 optDelay=5.697440587620612",
    "  branch[0] 'Deployable Payload' rows=642 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=263.27488781048834 tOptAlt=7.557440587620612 optDelay=5.697440587620612 sepTime=8.86 "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=7 withData=0 mutable=false",
    "  branch[1] 'Booster' rows=529 types=58 (builtin 58, other 0) srcId=null optAlt=NaN "
    "tOptAlt=NaN optDelay=NaN sepTime=8.86 events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, "
    "STAGE_SEPARATION=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=5 withData=0 mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Dual parachute deployment.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 25> kExampleSimulationLines07{
    "sim[0] 'Simulation 1' status=LOADED fcid=b38ea08b hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=592.721 maxVel=135.387 maxAcc=721.905 maxMach=0.398 tApogee=10.324 "
    "tFlight=65.294 vGround=4.635 vRod=53.045 vDeploy=23.611 optDelay=9.994914735848946",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=a631d393-6d31-4fac-b12f-c44c793dcbe8 "
    "desc='Recovery device deployment at high speed (23.6 m/s)'",
    "  branch[0] 'Sustainer' rows=499 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=592.7208227240553 tOptAlt=10.323914735848946 optDelay=9.994914735848946 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=1 mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=db9b08a1 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=697.16 maxVel=140.914 maxAcc=144.068 maxMach=0.415 tApogee=11.446 "
    "tFlight=70.791 vGround=4.77 vRod=29.935 vDeploy=23.571 optDelay=10.105668280123114",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=9ed64212-cfe0-4872-aca0-6b0c20f0646a "
    "desc='Recovery device deployment at high speed (23.6 m/s)'",
    "  branch[0] 'Sustainer' rows=534 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=697.159891569157 tOptAlt=11.445668280123114 optDelay=10.105668280123114 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=1 mutable=false",
    "sim[2] 'Simulation 3' status=LOADED fcid=de88d3eb hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=2223.846 maxVel=388.569 maxAcc=504.582 maxMach=1.147 tApogee=17.352 "
    "tFlight=129.906 vGround=5.006 vRod=54.125 vDeploy=24.978 optDelay=15.350000000000112",
    "  warning Other prio=NORMAL id=6ec220ba-aea5-4619-80e1-787a9a95952d desc='Body calculations "
    "may not be entirely accurate at supersonic speeds.'",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=f3d5d68c-ad5c-4a04-8e8b-b481c7f81b9d "
    "desc='Recovery device deployment at high speed (25 m/s)'",
    "  branch[0] 'Sustainer' rows=742 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=2223.84611454282 tOptAlt=17.40200000000011 optDelay=15.350000000000112 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, SIM_WARN=2, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=2 "
    "mutable=false",
    "sim[3] 'Simulation 4' status=LOADED fcid=098d8c95 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=897.473 maxVel=190.816 maxAcc=1003.51 maxMach=0.561 tApogee=12.194 "
    "tFlight=78.265 vGround=4.608 vRod=65.073 vDeploy=23.852 optDelay=11.915161975503638",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=2dbae6f2-6a17-4727-ba39-76451967ab1f "
    "desc='Recovery device deployment at high speed (23.9 m/s)'",
    "  branch[0] 'Sustainer' rows=561 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=897.4727559393039 tOptAlt=12.244161975503639 optDelay=11.915161975503638 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=1 mutable=false",
    "sim[4] 'Simulation 5' status=LOADED fcid=1654d8fb hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=1158.663 maxVel=242.744 maxAcc=867.184 maxMach=0.714 tApogee=13.443 "
    "tFlight=90.22 vGround=4.73 vRod=71.334 vDeploy=24.189 optDelay=13.153478792272534",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=61492123-8b79-4f97-b883-520b12fbb18e "
    "desc='Recovery device deployment at high speed (24.2 m/s)'",
    "  branch[0] 'Sustainer' rows=590 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=1158.6627918500167 tOptAlt=13.493478792272533 optDelay=13.153478792272534 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=1 mutable=false",
    "sim[5] 'Simulation 6' status=LOADED fcid=e4cf5b98 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=226.185 maxVel=58.427 maxAcc=61.849 maxMach=0.172 tApogee=7.34 "
    "tFlight=47.127 vGround=4.619 vRod=19.204 vDeploy=22.678 optDelay=4.899999999999983",
    "  warning RecoveryHighSpeedDeployment prio=NORMAL id=11c81537-6ca0-4545-8455-8bdf38cd6140 "
    "desc='Recovery device deployment at high speed (22.7 m/s)'",
    "  branch[0] 'Sustainer' rows=407 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=226.18487667778533 tOptAlt=7.339999999999983 optDelay=4.899999999999983 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, SIM_WARN=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=1 mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Parallel booster staging.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 7> kExampleSimulationLines08{
    "sim[0] 'Simulation 1' status=LOADED fcid=6e2539fb hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=1120.361 maxVel=210.802 maxAcc=132.783 maxMach=0.622 tApogee=13.049 "
    "tFlight=224.366 vGround=5.53 vRod=14.765 vDeploy=4.938500000000009 optDelay=9.584500000000057",
    "  branch[0] 'Sustainer' rows=1139 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=1120.3614421107316 tOptAlt=13.098500000000056 optDelay=9.584500000000057 sepTime=2.44 "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, EJECTION_CHARGE=2, "
    "STAGE_SEPARATION=1, APOGEE=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=10 withData=0 mutable=false",
    "  branch[1] 'Booster Set' rows=383 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=343.60394298756023 tOptAlt=4.324714226277397 optDelay=1.884714226277397 sepTime=2.44 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, TUMBLE=1, APOGEE=1, "
    "GROUND_HIT=1, SIMULATION_END=1} withSource=5 withData=0 mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=61807321 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=848.398 maxVel=188.761 maxAcc=116.692 maxMach=0.557 tApogee=11.149 "
    "tFlight=163.367 vGround=5.739 vRod=12.972 vDeploy=13.973500000000001 "
    "optDelay=7.634500000000029",
    "  branch[0] 'Sustainer' rows=862 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=848.3975925980817 tOptAlt=11.148500000000029 optDelay=7.634500000000029 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Pods--airframes and winglets.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 15> kExampleSimulationLines09{
    "sim[0] 'Simulation 1' status=LOADED fcid=5f9084c7 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=31.928 maxVel=24.753 maxAcc=123.728 maxMach=0.073 tApogee=2.807 "
    "tFlight=10.323 vGround=4.892 vRod=15.255 vDeploy=5.8610000000000015 "
    "optDelay=2.3128389790729544",
    "  branch[0] 'Sustainer' rows=255 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=31.928083437961995 tOptAlt=2.8468389790729542 optDelay=2.3128389790729544 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=a11cb733 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=89.669 maxVel=45.58 maxAcc=149.634 maxMach=0.135 tApogee=4.346 "
    "tFlight=24.876 vGround=4.923 vRod=17.209 vDeploy=4.380499999999997 "
    "optDelay=3.5235654328396646",
    "  branch[0] 'Sustainer' rows=392 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=89.6685561919072 tOptAlt=4.3835654328396645 optDelay=3.5235654328396646 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[2] 'Simulation 3' status=LOADED fcid=ebe43947 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=197.536 maxVel=71.885 maxAcc=164.48 maxMach=0.212 tApogee=6.213 "
    "tFlight=51.05 vGround=4.875 vRod=17.553 vDeploy=6.004999999999996 optDelay=4.387606893224738",
    "  branch[0] 'Sustainer' rows=549 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=197.53553825405544 tOptAlt=6.247606893224738 optDelay=4.387606893224738 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[3] 'Simulation 4' status=LOADED fcid=417f165a hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=206.121 maxVel=102.6 maxAcc=174.705 maxMach=0.302 tApogee=5.756 "
    "tFlight=51.776 vGround=5.052 vRod=17.196 vDeploy=9.517499999999995 "
    "optDelay=4.8674187721109075",
    "  branch[0] 'Sustainer' rows=591 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=206.12140060001983 tOptAlt=5.756418772110908 optDelay=4.8674187721109075 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[4] 'Simulation 5' status=LOADED fcid=cb521292 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=244.023 maxVel=126.858 maxAcc=216.152 maxMach=0.373 tApogee=5.976 "
    "tFlight=60.771 vGround=5.078 vRod=19.638 vDeploy=7.347500000000005 optDelay=5.137991723377971",
    "  branch[0] 'Sustainer' rows=624 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=244.02315080174736 tOptAlt=5.975991723377971 optDelay=5.137991723377971 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Pods--powered with recovery deployment.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 3> kExampleSimulationLines10{
    "sim[0] 'Simulation 1' status=LOADED fcid=0df5a831 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=90.703729 maxVel=43.272946 maxAcc=264.36964 maxMach=0.127581 "
    "tApogee=4.192304 tFlight=10.415242 vGround=18.227054 vRod=20.433528 "
    "vDeploy=1.3558539999999983 optDelay=3.123144531250001",
    "  branch[0] 'Sustainer' rows=374 types=70 (builtin 70, other 0) srcId=null "
    "optAlt=90.70606135216318 tOptAlt=4.183144531250001 optDelay=3.123144531250001 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=8 "
    "withData=0 mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Simulation extensions.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 13> kExampleSimulationLines11{
    "sim[0] 'No controlling' status=LOADED fcid=487170d2 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=2462.535 maxVel=240.707 maxAcc=52.815 maxMach=0.715 tApogee=22.461 "
    "tFlight=169.453 vGround=7.208 vRod=15.43 vDeploy=19.019 optDelay=12.674897279986228",
    "  branch[0] 'Primary stage' rows=1014 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=2462.534907456075 tOptAlt=22.460897279986227 optDelay=12.674897279986228 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[1] 'Active roll control' status=LOADED fcid=487170d2 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  ext id=info.openrocket.core.simulation.extension.example.RollControl name=Roll Control",
    "  summary maxAlt=2461.954 maxVel=240.707 maxAcc=52.812 maxMach=0.715 tApogee=22.436 "
    "tFlight=170.07 vGround=7.222 vRod=15.429 vDeploy=19.04 optDelay=12.700000000000179",
    "  branch[0] 'Primary stage' rows=935 types=59 (builtin 58, other 1) srcId=null "
    "optAlt=2461.9538975937744 tOptAlt=22.48600000000018 optDelay=12.700000000000179 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[2] 'Roll control + air-start' status=LOADED fcid=487170d2 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  ext id=info.openrocket.core.simulation.extension.example.RollControl name=Roll Control",
    "  ext id=info.openrocket.core.simulation.extension.example.AirStart name=Air-start (100 m, 50 "
    "m/s)",
    "  summary maxAlt=2916.968 maxVel=258.911 maxAcc=46.964 maxMach=0.771 tApogee=22.936 "
    "tFlight=191.505 vGround=7.226 vRod=49.856 vDeploy=19.011 optDelay=13.150000000000185",
    "  warning Other prio=LOW id=f4baad24-b9f7-4ad6-b777-a0dfbf9ebcb7 desc='Listeners modified the "
    "flight simulation'",
    "  branch[0] 'Primary stage' rows=1137 types=59 (builtin 58, other 1) srcId=null "
    "optAlt=2916.967866695541 tOptAlt=22.936000000000185 optDelay=13.150000000000185 sepTime=NaN "
    "events={SIM_WARN=1, LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=1 "
    "mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Simulation scripting.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 12> kExampleSimulationLines12{
    "sim[0] 'No controlling' status=LOADED fcid=487170d2 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=2465.451 maxVel=240.709 maxAcc=52.804 maxMach=0.715 tApogee=22.448 "
    "tFlight=169.59 vGround=7.218 vRod=15.429 vDeploy=19.005 optDelay=12.662430740167784",
    "  branch[0] 'Primary stage' rows=1093 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=2465.4513143242543 tOptAlt=22.448430740167783 optDelay=12.662430740167784 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[1] 'Active roll control' status=LOADED fcid=487170d2 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  ext id=info.openrocket.core.simulation.extension.impl.ScriptingExtension name=JavaScript "
    "script",
    "  summary maxAlt=2460.83 maxVel=240.692 maxAcc=52.811 maxMach=0.715 tApogee=22.43 "
    "tFlight=169.524 vGround=7.179 vRod=15.429 vDeploy=19.018 optDelay=12.643813686284949",
    "  branch[0] 'Primary stage' rows=1098 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=2460.8297152650193 tOptAlt=22.429813686284948 optDelay=12.643813686284949 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
    "sim[2] 'Roll control + air-start' status=LOADED fcid=487170d2 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  ext id=info.openrocket.core.simulation.extension.impl.ScriptingExtension name=JavaScript "
    "script",
    "  ext id=info.openrocket.core.simulation.extension.impl.ScriptingExtension name=JavaScript "
    "script",
    "  summary maxAlt=2460.598 maxVel=240.708 maxAcc=52.808 maxMach=0.715 tApogee=22.443 "
    "tFlight=169.432 vGround=7.266 vRod=9.837 vDeploy=19.016 optDelay=12.657499560766658",
    "  branch[0] 'Primary stage' rows=1078 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=2460.5976537906195 tOptAlt=22.443499560766657 optDelay=12.657499560766658 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Three stage low power rocket.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 15> kExampleSimulationLines13{
    "sim[0] 'Simulation 1' status=LOADED fcid=da326836 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=268.347 maxVel=78.37 maxAcc=127.66 maxMach=0.232 tApogee=7.64 tFlight=72.071 "
    "vGround=4.449 vRod=11.198 vDeploy=8.756666666666666 optDelay=5.32314453125",
    "  branch[0] 'Sustainer' rows=559 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=268.63559411487824 tOptAlt=7.76714453125 optDelay=5.32314453125 sepTime=1.714 "
    "events={LAUNCH=1, IGNITION=3, LIFTOFF=1, LAUNCHROD=1, BURNOUT=3, EJECTION_CHARGE=3, "
    "STAGE_SEPARATION=2, RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=14 withData=0 mutable=false",
    "  branch[1] 'Booster stage' rows=262 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=71.88519321824876 tOptAlt=3.0005135471330884 optDelay=1.2865135471330884 sepTime=1.714 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, TUMBLE=1, APOGEE=1, "
    "GROUND_HIT=1, SIMULATION_END=1} withSource=5 withData=0 mutable=false",
    "  branch[2] 'Booster stage' rows=186 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=26.74348103040621 tOptAlt=2.1468835201309004 optDelay=1.2898835201309005 sepTime=0.857 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, TUMBLE=1, APOGEE=1, "
    "GROUND_HIT=1, SIMULATION_END=1} withSource=5 withData=0 mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=05896b5a hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=479.475 maxVel=119.935 maxAcc=173.798 maxMach=0.354 tApogee=8.989 "
    "tFlight=123.969 vGround=4.539 vRod=10.785 vDeploy=17.441000000000003 "
    "optDelay=6.373144531250016",
    "  branch[0] 'Sustainer' rows=821 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=487.0877279387203 tOptAlt=9.947144531250016 optDelay=6.373144531250016 sepTime=1.714 "
    "events={LAUNCH=1, IGNITION=3, LIFTOFF=1, LAUNCHROD=1, BURNOUT=3, EJECTION_CHARGE=3, "
    "STAGE_SEPARATION=2, RECOVERY_DEVICE_DEPLOYMENT=1, APOGEE=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=14 withData=0 mutable=false",
    "  branch[1] 'Booster stage' rows=249 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=67.49430876968214 tOptAlt=2.941333978028314 optDelay=1.2273339780283141 sepTime=1.714 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, TUMBLE=1, APOGEE=1, "
    "GROUND_HIT=1, SIMULATION_END=1} withSource=5 withData=0 mutable=false",
    "  branch[2] 'Booster stage' rows=164 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=24.52268840998329 tOptAlt=2.0991148787088627 optDelay=1.2421148787088627 sepTime=0.857 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, TUMBLE=1, APOGEE=1, "
    "GROUND_HIT=1, SIMULATION_END=1} withSource=5 withData=0 mutable=false",
    "sim[2] 'Simulation 3' status=LOADED fcid=b13e8ca2 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=644.89 maxVel=129.625 maxAcc=143.679 maxMach=0.385 tApogee=11.937 "
    "tFlight=165.909 vGround=4.722 vRod=10.34 vDeploy=15.656333333333333 "
    "optDelay=6.407466310852863",
    "  branch[0] 'Sustainer' rows=1100 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=644.8895078405844 tOptAlt=11.987466310852863 optDelay=6.407466310852863 sepTime=3.72 "
    "events={LAUNCH=1, IGNITION=3, LIFTOFF=1, LAUNCHROD=1, BURNOUT=3, EJECTION_CHARGE=3, "
    "STAGE_SEPARATION=2, APOGEE=1, RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=14 withData=0 mutable=false",
    "  branch[1] 'Booster stage' rows=343 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=195.04438813926288 tOptAlt=5.057575296970981 optDelay=1.3375752969709809 sepTime=3.72 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, TUMBLE=1, APOGEE=1, "
    "GROUND_HIT=1, SIMULATION_END=1} withSource=5 withData=0 mutable=false",
    "  branch[2] 'Booster stage' rows=237 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=63.76488205798562 tOptAlt=3.210735557747543 optDelay=1.3507355577475428 sepTime=1.86 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, TUMBLE=1, APOGEE=1, "
    "GROUND_HIT=1, SIMULATION_END=1} withSource=5 withData=0 mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Tube fin rocket.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 3> kExampleSimulationLines14{
    "sim[0] 'Simulation 1' status=LOADED fcid=f58f38e4 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=282.234 maxVel=119.326 maxAcc=388.603 maxMach=0.351 tApogee=5.933 "
    "tFlight=75.614 vGround=4.406 vRod=18.871 vDeploy=9.048 optDelay=4.2825778317382",
    "  branch[0] 'Sustainer' rows=645 types=58 (builtin 58, other 0) srcId=null "
    "optAlt=282.2344136267147 tOptAlt=5.932577831738199 optDelay=4.2825778317382 sepTime=NaN "
    "events={LAUNCH=1, IGNITION=1, LIFTOFF=1, LAUNCHROD=1, BURNOUT=1, APOGEE=1, EJECTION_CHARGE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=0 "
    "mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// Two stage high power rocket.ork
// NOLINTBEGIN(modernize-raw-string-literal): a raw string literal cannot be broken into lines
inline constexpr std::array<std::string_view, 10> kExampleSimulationLines15{
    "sim[0] 'Simulation 1' status=LOADED fcid=cf7a0b8c hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=675.97 maxVel=158.916 maxAcc=110.229 maxMach=0.469 tApogee=11.049 "
    "tFlight=64.412 vGround=6.204 vRod=19.884 vDeploy=19.038 optDelay=8.028925750969263",
    "  warning Other prio=LOW id=4253f05e-8f66-4789-983f-b736abc6f615 desc='Open forward airframe "
    "(diameter > 0)'",
    "  branch[0] 'Sustainer' rows=439 types=65 (builtin 65, other 0) srcId=null "
    "optAlt=675.9701601976719 tOptAlt=11.098925750969263 optDelay=8.028925750969263 sepTime=1.535 "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, EJECTION_CHARGE=2, "
    "STAGE_SEPARATION=1, APOGEE=1, RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} "
    "withSource=11 withData=0 mutable=false",
    "  branch[1] 'Booster' rows=227 types=65 (builtin 65, other 0) srcId=null "
    "optAlt=167.99825801998898 tOptAlt=5.6815920007033425 optDelay=4.146592000703342 sepTime=1.535 "
    "events={IGNITION=1, BURNOUT=1, EJECTION_CHARGE=1, STAGE_SEPARATION=1, SIM_WARN=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 withData=1 "
    "mutable=false",
    "sim[1] 'Simulation 2' status=LOADED fcid=fadeeef3 hasSimData=true hasSummary=true "
    "seedFixed=false",
    "  summary maxAlt=1378.355 maxVel=175.735 maxAcc=116.584 maxMach=0.519 tApogee=16.275 "
    "tFlight=103.389 vGround=6.451 vRod=15.523 vDeploy=19.429 optDelay=6.156250000000007",
    "  warning Other prio=LOW id=4253f05e-8f66-4789-983f-b736abc6f615 desc='Open forward airframe "
    "(diameter > 0)'",
    "  branch[0] 'Sustainer' rows=617 types=65 (builtin 65, other 0) srcId=null "
    "optAlt=1378.3545862340281 tOptAlt=16.275250000000007 optDelay=6.156250000000007 sepTime=1.515 "
    "events={LAUNCH=1, IGNITION=2, LIFTOFF=1, LAUNCHROD=1, BURNOUT=2, STAGE_SEPARATION=1, "
    "APOGEE=1, RECOVERY_DEVICE_DEPLOYMENT=2, GROUND_HIT=1, SIMULATION_END=1} withSource=9 "
    "withData=0 mutable=false",
    "  branch[1] 'Booster' rows=278 types=65 (builtin 65, other 0) srcId=null "
    "optAlt=261.5187151402351 tOptAlt=6.2606482521671225 optDelay=4.745648252167123 sepTime=1.515 "
    "events={IGNITION=1, BURNOUT=1, STAGE_SEPARATION=1, SIM_WARN=1, APOGEE=1, "
    "RECOVERY_DEVICE_DEPLOYMENT=1, EJECTION_CHARGE=1, GROUND_HIT=1, SIMULATION_END=1} withSource=6 "
    "withData=1 mutable=false",
};
// NOLINTEND(modernize-raw-string-literal)

/// The sixteen examples, in the order of their file names as ExamplesProbe listed them. Together:
/// 54 simulations, 69 branches, 36962 rows, 735 events and 20 warnings.
inline constexpr std::array<ExampleSimulations, 16> kExampleSimulations{{
    {.file        = "3D printable nose cone and fins.ork",
     .lines       = kExampleSimulationLines00,
     .simulations = 5,
     .branches    = 5,
     .rows        = 1989,
     .events      = 54,
     .warnings    = 1},
    {.file        = "A simple model rocket.ork",
     .lines       = kExampleSimulationLines01,
     .simulations = 5,
     .branches    = 5,
     .rows        = 2580,
     .events      = 51,
     .warnings    = 1},
    {.file        = "ARC payload rocket.ork",
     .lines       = kExampleSimulationLines02,
     .simulations = 1,
     .branches    = 2,
     .rows        = 1413,
     .events      = 18,
     .warnings    = 0},
    {.file        = "Airstart timing.ork",
     .lines       = kExampleSimulationLines03,
     .simulations = 5,
     .branches    = 5,
     .rows        = 3265,
     .events      = 65,
     .warnings    = 5},
    {.file        = "Chute release.ork",
     .lines       = kExampleSimulationLines04,
     .simulations = 2,
     .branches    = 2,
     .rows        = 1014,
     .events      = 22,
     .warnings    = 0},
    {.file        = "Clustered motors.ork",
     .lines       = kExampleSimulationLines05,
     .simulations = 5,
     .branches    = 5,
     .rows        = 1512,
     .events      = 51,
     .warnings    = 1},
    {.file        = "Deployable payload.ork",
     .lines       = kExampleSimulationLines06,
     .simulations = 5,
     .branches    = 10,
     .rows        = 4205,
     .events      = 94,
     .warnings    = 2},
    {.file        = "Dual parachute deployment.ork",
     .lines       = kExampleSimulationLines07,
     .simulations = 6,
     .branches    = 6,
     .rows        = 3333,
     .events      = 67,
     .warnings    = 7},
    {.file        = "Parallel booster staging.ork",
     .lines       = kExampleSimulationLines08,
     .simulations = 2,
     .branches    = 3,
     .rows        = 2384,
     .events      = 32,
     .warnings    = 0},
    {.file        = "Pods--airframes and winglets.ork",
     .lines       = kExampleSimulationLines09,
     .simulations = 5,
     .branches    = 5,
     .rows        = 2411,
     .events      = 50,
     .warnings    = 0},
    {.file        = "Pods--powered with recovery deployment.ork",
     .lines       = kExampleSimulationLines10,
     .simulations = 1,
     .branches    = 1,
     .rows        = 374,
     .events      = 12,
     .warnings    = 0},
    {.file        = "Simulation extensions.ork",
     .lines       = kExampleSimulationLines11,
     .simulations = 3,
     .branches    = 3,
     .rows        = 3086,
     .events      = 31,
     .warnings    = 1},
    {.file        = "Simulation scripting.ork",
     .lines       = kExampleSimulationLines12,
     .simulations = 3,
     .branches    = 3,
     .rows        = 3269,
     .events      = 30,
     .warnings    = 0},
    {.file        = "Three stage low power rocket.ork",
     .lines       = kExampleSimulationLines13,
     .simulations = 3,
     .branches    = 9,
     .rows        = 3921,
     .events      = 102,
     .warnings    = 0},
    {.file        = "Tube fin rocket.ork",
     .lines       = kExampleSimulationLines14,
     .simulations = 1,
     .branches    = 1,
     .rows        = 645,
     .events      = 10,
     .warnings    = 0},
    {.file        = "Two stage high power rocket.ork",
     .lines       = kExampleSimulationLines15,
     .simulations = 2,
     .branches    = 4,
     .rows        = 1561,
     .events      = 46,
     .warnings    = 2},
}};

}  // namespace QtRocket::Test
