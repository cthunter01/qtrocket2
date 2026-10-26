package probe;

import java.io.File;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSet;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.utils.MotorCorrelation;

/**
 * OpenRocket's MotorCorrelation over the thrust curves of its motor database, one fact per line
 * (tools/openrocket-motor-chooser, family "correlation").
 * <p>
 * First the times at which crossCorrelation() samples the thrust: a Motor (a proxy) whose
 * getThrust() records the time asked and gives no thrust is correlated with itself.
 * <p>
 * The database is filled as the motor chooser and QtRocket's tests fill it:
 * ThrustCurveMotorSQLiteDatabase.readDatabase(), then ThrustCurveMotorSetDatabase.addMotor() for
 * every curve in the order read. For every motor set with more than one curve, in the order of
 * getMotorSets(), the probe prints the set, the digest of each curve in the order of getMotors(),
 * and for every ordered pair of two different curves MotorCorrelation.similarity() and
 * MotorCorrelation.crossCorrelation(). Every double is written with Double.toHexString (exact) and
 * Double.toString (Java 17).
 * <p>
 * Usage: java probe.MotorCorrelationProbe &lt;initial_motors.db&gt;
 * <p>
 * Lines (fields separated by a tab):
 * <pre>
 * SAMPLES    count (of the times per motor)  second time (hex)  last time (hex)  sum of the times in
 *            the order asked (hex)
 * DATABASE   curves  sets  motors
 * SET        setIndex  manufacturer (display name)  designation  curveCount
 * CURVE      setIndex  curveIndex  digest
 * PAIR       setIndex  first  second  similarity (hex)  similarity (toString)  crossCorrelation (hex)
 *            crossCorrelation (toString)
 * </pre>
 */
public class MotorCorrelationProbe {

	public static void main(String[] args) throws Exception {
		if (args.length != 1) {
			System.err.println("usage: java probe.MotorCorrelationProbe <initial_motors.db>");
			System.exit(2);
		}
		StringBuilder out = new StringBuilder();

		List<Double> times = new ArrayList<>();
		Motor recorder = (Motor) Proxy.newProxyInstance(Motor.class.getClassLoader(), new Class<?>[] { Motor.class },
				(proxy, method, arguments) -> {
					if (!method.getName().equals("getThrust")) {
						throw new UnsupportedOperationException(method.getName());
					}
					times.add((Double) arguments[0]);
					return 0.0;
				});
		MotorCorrelation.crossCorrelation(recorder, recorder);
		// Both motors are the recorder: every time is asked twice in a row.
		double sum = 0;
		for (int i = 0; i < times.size(); i += 2) {
			if (!times.get(i).equals(times.get(i + 1))) {
				throw new IllegalStateException("the motors were asked at different times");
			}
			sum += times.get(i);
		}
		out.append("SAMPLES\t").append(times.size() / 2).append('\t').append(Double.toHexString(times.get(2)))
				.append('\t').append(Double.toHexString(times.get(times.size() - 1))).append('\t')
				.append(Double.toHexString(sum)).append('\n');

		List<ThrustCurveMotor> curves = ThrustCurveMotorSQLiteDatabase.readDatabase(new File(args[0]));
		ThrustCurveMotorSetDatabase database = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor curve : curves) {
			database.addMotor(curve);
		}
		List<ThrustCurveMotorSet> sets = database.getMotorSets();
		int motors = 0;
		for (ThrustCurveMotorSet set : sets) {
			motors += set.getMotorCount();
		}
		out.append("DATABASE\t").append(curves.size()).append('\t').append(sets.size()).append('\t').append(motors)
				.append('\n');
		for (int setIndex = 0; setIndex < sets.size(); setIndex++) {
			ThrustCurveMotorSet set = sets.get(setIndex);
			List<ThrustCurveMotor> list = set.getMotors();
			if (list.size() < 2) {
				continue;
			}
			out.append("SET\t").append(setIndex).append('\t').append(set.getManufacturer().getDisplayName())
					.append('\t').append(set.getDesignation()).append('\t').append(list.size()).append('\n');
			for (int i = 0; i < list.size(); i++) {
				out.append("CURVE\t").append(setIndex).append('\t').append(i).append('\t')
						.append(list.get(i).getDigest()).append('\n');
			}
			for (int i = 0; i < list.size(); i++) {
				for (int j = 0; j < list.size(); j++) {
					if (i == j) {
						continue;
					}
					double similarity = MotorCorrelation.similarity(list.get(i), list.get(j));
					double cross = MotorCorrelation.crossCorrelation(list.get(i), list.get(j));
					out.append("PAIR\t").append(setIndex).append('\t').append(i).append('\t').append(j)
							.append('\t').append(Double.toHexString(similarity)).append('\t')
							.append(Double.toString(similarity)).append('\t').append(Double.toHexString(cross))
							.append('\t').append(Double.toString(cross)).append('\n');
				}
			}
		}
		System.out.print(out);
		System.out.flush();
	}
}
