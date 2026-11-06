-- wrk script (more-dims-20261106/n3-tail): WRK_DELAY_MS > 0 makes each connection wait that
-- long after every response (an offered load below the server's capacity); done() prints the
-- latency percentiles (us), mean, max and the counts.
local d = tonumber(os.getenv("WRK_DELAY_MS") or "0")
if d > 0 then
  function delay() return d end
end
done = function(summary, latency, requests)
  for _, p in ipairs({50, 90, 99, 99.9, 99.99}) do
    io.write(string.format("lat_p%s %d\n", p, latency:percentile(p)))
  end
  local e = summary.errors
  io.write(string.format("lat_max %d\nlat_mean %.1f\nreq_total %d\nerr_total %d\n",
    latency.max, latency.mean, summary.requests,
    e.connect + e.read + e.write + e.status + e.timeout))
end
