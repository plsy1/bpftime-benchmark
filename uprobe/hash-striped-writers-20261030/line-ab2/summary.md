
Churn 4 B key, 8 B value: map cost (total), us; reps of total in brackets
  threads 1
    c98951a4       0.129 (0.571)  [0.568, 0.571, 0.573]
    stripe-tokens  0.186 (0.632)  [0.632, 0.632, 0.634]
    stripe-view    0.159 (0.610)  [0.608, 0.610, 0.616]
    stripe-line    0.163 (0.613)  [0.604, 0.613, 0.614]
  threads 2
    c98951a4       0.207 (0.478)  [0.478, 0.478, 0.485]
    stripe-tokens  0.093 (0.364)  [0.363, 0.364, 0.364]
    stripe-view    0.075 (0.348)  [0.348, 0.348, 0.348]
    stripe-line    0.074 (0.346)  [0.346, 0.346, 0.350]
  threads 6
    c98951a4       1.814 (2.644)  [2.634, 2.644, 2.660]
    stripe-tokens  0.015 (0.852)  [0.852, 0.852, 0.862]
    stripe-view    0.020 (0.848)  [0.848, 0.848, 0.859]
    stripe-line    0.011 (0.841)  [0.841, 0.841, 0.844]

Tracking table, 1,000 live: map cost (total), us; reps of total in brackets
  threads 1
    c98951a4       0.493 (0.982)  [0.977, 0.982, 0.988]
    stripe-tokens  0.548 (1.039)  [1.039, 1.039, 1.041]
    stripe-view    0.587 (1.076)  [1.073, 1.076, 1.087]
    stripe-line    0.513 (1.005)  [1.005, 1.005, 1.006]

Tracking table, 900,000 live: map cost (total), us; reps of total in brackets
  threads 1
    c98951a4       0.753 (1.237)  [1.237, 1.237, 1.238]
    stripe-tokens  0.800 (1.288)  [1.284, 1.288, 1.291]
    stripe-view    0.774 (1.260)  [1.260, 1.260, 1.263]
    stripe-line    0.749 (1.236)  [1.235, 1.236, 1.258]

perf stat, tracking 1K, 1 thread: instructions / cycles per op
  c98951a4       2923 / 1693
  stripe-view    3134 / 1865
  stripe-line    3069 / 1740
  c98951a4       2922 / 1708
  stripe-view    3131 / 1855
  stripe-line    3072 / 1776

perf stat, churn, 1 thread (4 s window): instructions / cycles / l1d_tlb_refill per call
  c98951a4.0.churn.stat.txt: 2581 / 984 / 34.7
  c98951a4.2.churn.stat.txt: 2582 / 983 / 34.9
  stripe-line.1.churn.stat.txt: 2719 / 1046 / 52.4
  stripe-line.3.churn.stat.txt: 2719 / 1042 / 52.4
