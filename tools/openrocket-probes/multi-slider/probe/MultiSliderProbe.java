package probe;

import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.ComponentEvent;
import java.awt.event.InputEvent;
import java.awt.event.MouseEvent;
import java.awt.event.MouseWheelEvent;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Hashtable;
import java.util.List;

import javax.swing.InputMap;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.KeyStroke;
import javax.swing.SwingUtilities;

import com.formdev.flatlaf.FlatLightLaf;

import info.openrocket.swing.gui.widgets.MultiSlider;

/**
 * Milestone 3 run 2, part M1: OpenRocket's two-thumb MultiSlider (swing gui/widgets/MultiSlider.java and
 * MultiSliderUI.java) as MotorFilterPanel makes its three (bounded; the impulse slider 0..14 with a tick
 * every value and the labels A..O, the length slider 0..1000 without either), under FlatLightLaf as
 * OpenRocket sets it up on Linux, headless: the key bindings, the sizes, and what the keys, presses,
 * drags and held presses do to the two thumbs, with the slider's change events (the values and the
 * adjusting flags after each). The slider is 300 pixels wide; a thumb's x is the centre of its rectangle.
 * tests/gui/widgets/RangeSliderTests.cpp pins what it prints by hand (expected.txt; see ../README.md).
 * The held presses print the time between the steps rounded to 100 ms.
 */
public class MultiSliderProbe {
	static final String[] IMPULSE = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O" };
	static final int WIDTH = 300;

	static MultiSlider slider;
	/** A slider made as the probed one, whose thumb 0 tells where a value's thumb lies. */
	static MultiSlider ruler;
	static final List<String> events = new ArrayList<>();
	static final List<Long> times = new ArrayList<>();
	static long t0;

	static void p(String s) {
		System.out.println(s);
	}

	static Rectangle[] thumbsOf(MultiSlider s) throws Exception {
		Field f = s.getUI().getClass().getDeclaredField("thumbRects");
		f.setAccessible(true);
		return (Rectangle[]) f.get(s.getUI());
	}

	static Rectangle[] thumbs() throws Exception {
		return thumbsOf(slider);
	}

	/** The x of the centre of a thumb at value. */
	static int xFor(int value) throws Exception {
		ruler.setValueAt(0, value);
		Rectangle r = thumbsOf(ruler)[0];
		return r.x + r.width / 2;
	}

	static int thumbY() throws Exception {
		Rectangle r = thumbs()[0];
		return r.y + r.height / 2;
	}

	static String state() throws Exception {
		Rectangle[] r = thumbs();
		return "values=" + slider.getValueAt(0) + "," + slider.getValueAt(1) + " current=" + slider.getCurrentThumbIndex()
				+ " thumb0=" + r[0].x + ".." + (r[0].x + r[0].width - 1) + " thumb1=" + r[1].x + ".." + (r[1].x + r[1].width - 1);
	}

	static void mouse(int id, int x, int y, int button, int modifiers) {
		slider.dispatchEvent(new MouseEvent(slider, id, System.currentTimeMillis(), modifiers, x, y, 1, false, button));
	}

	static void press(int x) throws Exception {
		mouse(MouseEvent.MOUSE_PRESSED, x, thumbY(), MouseEvent.BUTTON1, InputEvent.BUTTON1_DOWN_MASK);
	}

	static void drag(int x) throws Exception {
		mouse(MouseEvent.MOUSE_DRAGGED, x, thumbY(), MouseEvent.NOBUTTON, InputEvent.BUTTON1_DOWN_MASK);
	}

	static void release(int x) throws Exception {
		mouse(MouseEvent.MOUSE_RELEASED, x, thumbY(), MouseEvent.BUTTON1, 0);
	}

	static void action(String name) {
		slider.getActionMap().get(name).actionPerformed(new ActionEvent(slider, ActionEvent.ACTION_PERFORMED, name));
	}

	static MultiSlider make(int max, boolean labels) {
		MultiSlider s = new MultiSlider(MultiSlider.HORIZONTAL, 0, max, 0, max);
		s.setBounded(true);
		if (labels) {
			s.setMajorTickSpacing(1);
			s.setPaintTicks(true);
			Hashtable<Integer, JLabel> table = new Hashtable<>();
			for (int i = 0; i < IMPULSE.length; i++) {
				table.put(i, new JLabel(IMPULSE[i]));
			}
			s.setLabelTable(table);
			s.setPaintLabels(true);
		}
		return s;
	}

	static void layOut(MultiSlider s) {
		s.setSize(WIDTH, s.getPreferredSize().height);
		s.dispatchEvent(new ComponentEvent(s, ComponentEvent.COMPONENT_RESIZED));
	}

	static void build(int max, boolean labels) throws Exception {
		slider = make(max, labels);
		ruler = make(max, labels);
		p("== slider 0.." + max + (labels ? " with ticks and labels" : "") + ": preferred " + slider.getPreferredSize().width + "x"
				+ slider.getPreferredSize().height + ", minimum " + slider.getMinimumSize().width + "x" + slider.getMinimumSize().height);
		layOut(slider);
		layOut(ruler);
		slider.addChangeListener(e -> {
			times.add(System.currentTimeMillis() - t0);
			events.add(slider.getValueAt(0) + "," + slider.getValueAt(1) + " adjusting=" + slider.getValueIsAdjustingAt(0) + ","
					+ slider.getValueIsAdjustingAt(1));
		});
		StringBuilder sb = new StringBuilder("thumb x of each value:");
		for (int v = 0; v <= max; v += max > 20 ? 100 : 1) {
			sb.append(' ').append(v).append('=').append(xFor(v));
		}
		Rectangle r = thumbs()[0];
		p(sb + "; thumb " + r.width + "x" + r.height + " at y " + r.y + ", slider " + slider.getWidth() + "x" + slider.getHeight());
	}

	static void events(String title) {
		p("  events of " + title + ": " + (events.isEmpty() ? "none" : String.join(" | ", events)));
		events.clear();
		times.clear();
	}

	static void timedEvents(String title) {
		List<String> gaps = new ArrayList<>();
		for (int i = 1; i < times.size(); i++) {
			gaps.add(String.valueOf(Math.round((times.get(i) - times.get(i - 1)) / 100.0) * 100));
		}
		p("  events of " + title + ": " + String.join(" | ", events));
		p("  ms between them (rounded): " + String.join(" ", gaps));
		events.clear();
		times.clear();
	}

	static void set(int v0, int v1) {
		slider.setValueAt(0, v0);
		slider.setValueAt(1, v1);
		events.clear();
		times.clear();
		t0 = System.currentTimeMillis();
	}

	static void edt(ThrowingRunnable r) throws Exception {
		Exception[] ex = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				r.run();
			} catch (Exception e) {
				ex[0] = e;
			}
		});
		if (ex[0] != null) {
			throw ex[0];
		}
	}

	interface ThrowingRunnable {
		void run() throws Exception;
	}

	static void keys() throws Exception {
		InputMap map = slider.getInputMap(JComponent.WHEN_FOCUSED);
		List<String> lines = new ArrayList<>();
		for (KeyStroke k : map.allKeys()) {
			lines.add(k + " -> " + map.get(k));
		}
		Collections.sort(lines);
		p("key bindings (focused): " + String.join("; ", lines));
		p("focusable: " + slider.isFocusable());
		set(0, 14);
		action("positiveUnitIncrement");
		p("key +unit on thumb 0: " + state());
		action("positiveBlockIncrement");
		p("key +block: " + state());
		action("maxScroll");
		p("key max: " + state());
		action("positiveUnitIncrement");
		p("key +unit at the maximum: " + state());
		action("minScroll");
		p("key min: " + state());
		action("negativeUnitIncrement");
		p("key -unit at the minimum: " + state());
		action("negativeBlockIncrement");
		p("key -block at the minimum: " + state());
		events("the keys");
		set(5, 7);
		Rectangle r = thumbs()[1];
		press(r.x + r.width / 2);
		release(r.x + r.width / 2);
		p("click on thumb 1 at 5,7: " + state());
		events("the click");
		for (int i = 0; i < 4; i++) {
			action("negativeUnitIncrement");
			p("key -unit on thumb 1: " + state());
		}
		events("the keys");
		slider.setValueAt(0, 9);
		p("setValueAt(0, 9): " + state());
		slider.setValueAt(1, 20);
		p("setValueAt(1, 20): " + state());
		slider.setValueAt(0, -3);
		p("setValueAt(0, -3): " + state());
		events("the sets");
		slider.setValueAt(0, 0);
		events("setValueAt(0, 0) again");
	}

	static void drags() throws Exception {
		set(2, 6);
		Rectangle r = thumbs()[0];
		int cx = r.x + r.width / 2;
		press(cx);
		p("press on thumb 0 at 2,6: " + state());
		drag(cx + 10);
		p("drag by +10: " + state());
		drag(cx + 200);
		p("drag by +200: " + state());
		drag(cx - 500);
		p("drag by -500: " + state());
		release(cx - 500);
		p("release: " + state());
		events("the drag");
		for (int[] move : new int[][] { { -30, 30 }, { 30, 0 }, { 0, 30 } }) {
			set(7, 7);
			cx = xFor(7);
			press(cx);
			p("press on both thumbs at 7,7: " + state());
			drag(cx + move[0]);
			p("drag by " + move[0] + ": " + state());
			if (move[1] != 0) {
				drag(cx + move[1]);
				p("then by " + move[1] + ": " + state());
			}
			release(cx + move[1]);
			events("the drag");
		}
	}

	static void trackPresses() throws Exception {
		for (int v : new int[] { 1, 5, 6, 7, 8, 9, 13 }) {
			set(3, 11);
			press(xFor(v));
			p("press at " + v + " (x " + xFor(v) + ") beside the thumbs at 3,11: " + state());
			release(xFor(v));
			events("the press and its release");
		}
		set(6, 8);
		Rectangle[] r = thumbs();
		int leftEnd = r[0].x + r[0].width;
		int rightStart = r[1].x;
		p("thumbs at 6,8: thumb 0 ends before x " + leftEnd + ", thumb 1 starts at x " + rightStart);
		for (int x : new int[] { (leftEnd + rightStart) / 2, leftEnd + 1, rightStart - 1 }) {
			set(6, 8);
			press(x);
			p("press at x " + x + ": " + state());
			release(x);
		}
		set(7, 7);
		press(xFor(12));
		p("press at 12 beside the thumbs at 7,7: " + state());
		release(xFor(12));
		set(7, 7);
		press(xFor(2));
		p("press at 2 beside the thumbs at 7,7: " + state());
		release(xFor(2));
		set(3, 11);
		press(1);
		p("press at x 1, thumbs at 3,11: " + state());
		release(1);
		set(3, 11);
		press(WIDTH - 2);
		p("press at x " + (WIDTH - 2) + ", thumbs at 3,11: " + state());
		release(WIDTH - 2);
		for (int v : new int[] { 3, 11, 7 }) {
			set(3, 11);
			int y = slider.getHeight() - 3;
			mouse(MouseEvent.MOUSE_PRESSED, xFor(v), y, MouseEvent.BUTTON1, InputEvent.BUTTON1_DOWN_MASK);
			p("press in the labels under " + v + " (y " + y + "), thumbs at 3,11: " + state());
			mouse(MouseEvent.MOUSE_RELEASED, xFor(v), y, MouseEvent.BUTTON1, 0);
		}
	}

	static void otherInput() throws Exception {
		set(0, 14);
		int x = xFor(7);
		mouse(MouseEvent.MOUSE_PRESSED, x, thumbY(), MouseEvent.BUTTON3, InputEvent.BUTTON3_DOWN_MASK);
		mouse(MouseEvent.MOUSE_RELEASED, x, thumbY(), MouseEvent.BUTTON3, 0);
		p("right button at 7, thumbs at 0,14: " + state());
		mouse(MouseEvent.MOUSE_PRESSED, x, thumbY(), MouseEvent.BUTTON2, InputEvent.BUTTON2_DOWN_MASK);
		mouse(MouseEvent.MOUSE_RELEASED, x, thumbY(), MouseEvent.BUTTON2, 0);
		p("then the middle button at 7: " + state());
		set(0, 14);
		int cx = xFor(0);
		mouse(MouseEvent.MOUSE_PRESSED, cx, thumbY(), MouseEvent.BUTTON3, InputEvent.BUTTON3_DOWN_MASK);
		mouse(MouseEvent.MOUSE_DRAGGED, xFor(3), thumbY(), MouseEvent.NOBUTTON, InputEvent.BUTTON3_DOWN_MASK);
		mouse(MouseEvent.MOUSE_RELEASED, xFor(3), thumbY(), MouseEvent.BUTTON3, 0);
		p("right-button drag of thumb 0 from 0 to 3: " + state());
		set(0, 14);
		slider.dispatchEvent(new MouseWheelEvent(slider, MouseEvent.MOUSE_WHEEL, System.currentTimeMillis(), 0, x, thumbY(), 0, false,
				MouseWheelEvent.WHEEL_UNIT_SCROLL, 3, -1));
		p("wheel up at 7: " + state());
		events("the wheel");
		set(3, 11);
		slider.setEnabled(false);
		press(xFor(7));
		release(xFor(7));
		Rectangle r = thumbs()[0];
		press(r.x + r.width / 2);
		drag(r.x + r.width / 2 + 40);
		release(r.x + r.width / 2 + 40);
		slider.setEnabled(true);
		p("disabled: a press at 7 and a drag of thumb 0: " + state());
		events("the disabled slider");
	}

	static void hold(int fromV0, int fromV1, int pressValue, Integer moveTo, long moveAfter, long total) throws Exception {
		edt(() -> {
			set(fromV0, fromV1);
			press(xFor(pressValue));
			p("hold at " + pressValue + ", thumbs at " + fromV0 + "," + fromV1 + ": " + state());
		});
		if (moveTo != null) {
			Thread.sleep(moveAfter);
			edt(() -> drag(moveTo == Integer.MAX_VALUE ? WIDTH - 1 : xFor(moveTo)));
		} else {
			Thread.sleep(200);
			edt(() -> p("  after 200 ms: " + state()));
		}
		Thread.sleep(total);
		edt(() -> {
			p("  " + (moveTo == null ? "" : "moved to " + (moveTo == Integer.MAX_VALUE ? "x " + (WIDTH - 1) : moveTo) + " after " + moveAfter
					+ " ms, ") + "after " + total + " ms more: " + state());
			release(xFor(pressValue));
			timedEvents("the hold");
		});
	}

	static void lengthSlider() throws Exception {
		edt(() -> {
			build(1000, false);
			set(0, 1000);
			action("positiveBlockIncrement");
			p("key +block: " + state());
			action("positiveUnitIncrement");
			p("key +unit: " + state());
			press(xFor(500));
			p("press at 500: " + state());
			release(xFor(500));
			events("the keys and the press");
			for (int v : new int[] { 503, 507, 251, 999 }) {
				set(0, v);
				Rectangle r = thumbs()[0];
				int cx = r.x + r.width / 2;
				press(cx);
				drag(cx + 400);
				p("drag of thumb 0 onto thumb 1 at 0," + v + ": " + state());
				release(cx + 400);
				set(v, 1000);
				r = thumbs()[1];
				cx = r.x + r.width / 2;
				press(cx);
				drag(cx - 400);
				p("drag of thumb 1 onto thumb 0 at " + v + ",1000: " + state());
				release(cx - 400);
			}
		});
		for (int off : new int[] { 0, 5, 6, 7 }) {
			edt(() -> {
				set(0, 1000);
				press(xFor(400) + off);
			});
			Thread.sleep(1000);
			edt(() -> {
				p("hold at x " + (xFor(400) + off) + " (400 and " + off + " px), thumbs at 0,1000: " + state());
				release(xFor(400) + off);
				set(0, 1000);
				press(xFor(600) - off);
			});
			Thread.sleep(1000);
			edt(() -> {
				p("hold at x " + (xFor(600) - off) + " (600 less " + off + " px), thumbs at 0,1000: " + state());
				release(xFor(600) - off);
				events.clear();
				times.clear();
			});
		}
	}

	static void constructors() {
		for (int[] v : new int[][] { { 20, -5 }, { 9, 3 } }) {
			try {
				MultiSlider c = new MultiSlider(MultiSlider.HORIZONTAL, 0, 14, v[0], v[1]);
				p("new MultiSlider(0, 14, " + v[0] + ", " + v[1] + "): " + c.getValueAt(0) + "," + c.getValueAt(1));
			} catch (Exception e) {
				p("new MultiSlider(0, 14, " + v[0] + ", " + v[1] + "): " + e);
			}
		}
	}

	public static void main(String[] args) throws Exception {
		FlatLightLaf.setup();
		edt(() -> {
			build(14, true);
			keys();
			drags();
			trackPresses();
			otherInput();
		});
		hold(0, 14, 10, null, 0, 1300);
		hold(0, 14, 4, 12, 250, 1500);
		hold(0, 14, 10, Integer.MAX_VALUE, 80, 700);
		hold(3, 8, 6, 1, 80, 1000);
		lengthSlider();
		edt(MultiSliderProbe::constructors);
		System.exit(0);
	}
}
