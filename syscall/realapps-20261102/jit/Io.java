// Java: JIT-compiled hot loop, threads, file I/O.
import java.nio.file.*;
public class Io {
    public static void main(String[] a) throws Exception {
        long s = 0;
        for (int i = 0; i < 200_000_000; i++) s = (s + i * 7L) % 1000003;
        final long[] n = new long[8];
        Thread[] t = new Thread[8];
        for (int k = 0; k < 8; k++) { final int id = k; t[k] = new Thread(() -> {
            for (int i = 0; i < 2000; i++) { try { Files.readAllBytes(Paths.get("/proc/self/stat")); n[id]++; } catch (Exception e) {} } }); t[k].start(); }
        long tot = 0; for (int k = 0; k < 8; k++) { t[k].join(); tot += n[k]; }
        System.out.println("ok " + s + " " + tot);
    }
}
