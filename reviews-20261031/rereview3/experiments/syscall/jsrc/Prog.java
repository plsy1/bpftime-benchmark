import java.util.*; import java.util.concurrent.*; import java.io.*;
public class Prog { public static void main(String[] a) throws Exception {
  ExecutorService ex = Executors.newFixedThreadPool(8); List<Future<Long>> fs = new ArrayList<>();
  for (int i = 0; i < 32; i++) fs.add(ex.submit(() -> { long s = 0; for (int j = 0; j < 50; j++) { s += new File("/proc/self/stat").length(); try (FileInputStream in = new FileInputStream("/proc/self/stat")) { s += in.read(new byte[64]); } } return s; }));
  for (Future<Long> f : fs) f.get(); ex.shutdown();
  for (int i = 0; i < 10; i++) { Process p = new ProcessBuilder("/bin/echo", "hi").redirectErrorStream(true).start(); String out = new String(p.getInputStream().readAllBytes()); if (p.waitFor() != 0 || !out.equals("hi\n")) { System.out.println("spawn failed"); System.exit(1); } }
  System.out.println("java OK"); } }
