// E5-real workload J: 4 named threads write 2000 lines each with unbuffered FileOutputStream (one write(2) per
// line); e5-worker-1 renames itself to e5-renamed after 1000 lines (Thread.setName on the current thread, which
// HotSpot passes to pthread_setname_np). The main thread writes 100 lines before and after.
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class E5Threads {
    static void lines(FileOutputStream out, String tag, int n) throws IOException {
        for (int i = 0; i < n; i++)
            out.write((tag + " " + i + "\n").getBytes(StandardCharsets.US_ASCII));
    }

    public static void main(String[] args) throws Exception {
        String dir = args[0];
        try (FileOutputStream m = new FileOutputStream(dir + "/main.out")) {
            lines(m, "main-before", 100);
            Thread[] ts = new Thread[4];
            for (int k = 0; k < 4; k++) {
                final int id = k + 1;
                ts[k] = new Thread(() -> {
                    try (FileOutputStream o = new FileOutputStream(dir + "/w" + id + ".out")) {
                        if (id == 1) {
                            lines(o, "w1", 1000);
                            Thread.currentThread().setName("e5-renamed");
                            lines(o, "w1r", 1000);
                        } else {
                            lines(o, "w" + id, 2000);
                        }
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                }, "e5-worker-" + id);
                ts[k].start();
            }
            for (Thread t : ts)
                t.join();
            lines(m, "main-after", 100);
        }
    }
}
