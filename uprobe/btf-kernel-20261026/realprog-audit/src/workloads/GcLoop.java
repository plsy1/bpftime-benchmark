// javagc target: waits DELAY seconds (the tool needs the JVM's pid before it attaches), then
// calls System.gc() COUNT times with some garbage in between and prints the collections the
// JVM itself counted (GarbageCollectorMXBean), to compare with the gc__begin/gc__end events.
import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;

public class GcLoop {
    static long collections() {
        long n = 0;
        for (GarbageCollectorMXBean gc : ManagementFactory.getGarbageCollectorMXBeans())
            n += Math.max(gc.getCollectionCount(), 0);
        return n;
    }

    public static void main(String[] args) throws Exception {
        int delay = args.length > 0 ? Integer.parseInt(args[0]) : 3;
        int count = args.length > 1 ? Integer.parseInt(args[1]) : 50;
        System.out.println("ready " + ProcessHandle.current().pid());
        System.out.flush();
        Thread.sleep(delay * 1000L);
        long before = collections();
        long sink = 0;
        for (int i = 0; i < count; i++) {
            byte[][] garbage = new byte[64][];
            for (int j = 0; j < garbage.length; j++) garbage[j] = new byte[16384];
            sink += garbage[i % garbage.length].length;
            System.gc();
        }
        System.out.println("sink " + sink);
        System.out.println("gc_collections " + (collections() - before));
        System.out.println("total_ops " + count);
    }
}
