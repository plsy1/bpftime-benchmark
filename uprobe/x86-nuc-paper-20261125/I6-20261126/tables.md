## C1-20261125-try1 (n rows 252)
| leg | n | server Rss MB | server Pss MB | server Private_Dirty MB | consumer Rss MB | consumer Pss MB | consumer Private_Dirty MB |
|---|---:|---:|---:|---:|---:|---:|---:|
| Dn | 21 | 20.2 | 11.8 | 4.9 | 45.5 | 42.2 | 7.6 |
| Dn+BPFTIME_CONSUMER_NAP_US=0 | 21 | 20.1 | 11.8 | 4.9 | 45.4 | 42.2 | 7.7 |
| Dn+BPFTIME_CONSUMER_NAP_US=0+SSLSNIFF_PB_WAKEUP=32 | 21 | 20.2 | 11.9 | 4.9 | 45.3 | 42.1 | 7.6 |
| Dn730 | 21 | 20.3 | 11.9 | 4.9 | 45.8 | 42.5 | 7.6 |
| DnR | 21 | 20.4 | 12.0 | 4.9 | 45.7 | 42.4 | 7.6 |
| K | 21 | 10.5 | 7.1 | 4.8 | 2.6 | 1.3 | 0.3 |
| K+SSLSNIFF_PB_WAKEUP=32 | 21 | 10.6 | 7.1 | 4.8 | 2.6 | 1.3 | 0.3 |
| Ol | 21 | 22.5 | 13.0 | 5.0 | 27.1 | 24.1 | 3.9 |
| Ol730x | 21 | 22.4 | 13.0 | 5.0 | 27.2 | 24.2 | 3.9 |
| OlL3 | 21 | 22.4 | 13.0 | 5.0 | 27.2 | 24.1 | 3.9 |
| OlL3x730 | 21 | 22.4 | 13.0 | 5.0 | 27.2 | 24.2 | 3.9 |
| OlL3x730R | 21 | 22.4 | 13.0 | 5.0 | 27.5 | 24.5 | 3.9 |
## H7a-20261126-try1 (n rows 143)
| leg | n | server Rss MB | server Pss MB | server Private_Dirty MB | consumer Rss MB | consumer Pss MB | consumer Private_Dirty MB |
|---|---:|---:|---:|---:|---:|---:|---:|
| B#H1 | 11 | 88.2 | 85.6 | 77.4 | nan | nan | nan |
| Dn#H1 | 11 | 126.4 | 121.9 | 84.4 | 45.5 | 42.4 | 7.6 |
| Dnpc#H1 | 11 | 126.4 | 121.9 | 84.5 | 45.5 | 42.5 | 7.6 |
| Dnpc#H1+BPFTIME_CONSUMER_NAP_US=0+SSLSNIFF_PB_WAKEUP=32 | 11 | 126.8 | 122.2 | 84.5 | 45.5 | 42.4 | 7.6 |
| Dxn#H1+BPFTIME_DIAG_EXACT=1 | 11 | 127.0 | 122.4 | 84.5 | 45.6 | 42.5 | 7.6 |
| K#H1 | 11 | 89.8 | 87.0 | 79.0 | 2.6 | 1.4 | 0.3 |
| K#H1+SSLSNIFF_PB_WAKEUP=32 | 11 | 88.2 | 85.4 | 77.4 | 2.6 | 1.4 | 0.3 |
| L3port#H1 | 11 | 140.1 | 135.8 | 87.0 | 28.1 | 25.2 | 4.7 |
| O#H1 | 11 | 140.4 | 135.9 | 86.9 | 27.4 | 24.5 | 4.1 |
| Ol#H1 | 11 | 139.7 | 135.3 | 87.0 | 27.3 | 24.3 | 3.9 |
| OlL3x730#H1 | 11 | 139.7 | 135.3 | 86.9 | 27.3 | 24.3 | 3.9 |
| OlL3x730pc#H1 | 11 | 139.7 | 135.3 | 87.0 | 27.2 | 24.3 | 3.8 |
| Olpc#H1 | 11 | 139.6 | 135.2 | 86.9 | 27.4 | 24.4 | 3.9 |
## H7b-20261126-try1 (n rows 189)
| leg | n | server Rss MB | server Pss MB | server Private_Dirty MB | consumer Rss MB | consumer Pss MB | consumer Private_Dirty MB |
|---|---:|---:|---:|---:|---:|---:|---:|
| B#H4 | 21 | 88.2 | 85.5 | 77.4 | nan | nan | nan |
| Dn#H4 | 21 | 127.2 | 122.3 | 84.5 | 45.7 | 42.3 | 7.4 |
| Dn730f#H4 | 21 | 127.5 | 122.6 | 84.6 | 46.0 | 42.6 | 7.4 |
| Dnpc#H4 | 21 | 127.2 | 122.3 | 84.5 | 45.7 | 42.4 | 7.4 |
| K#H4 | 21 | 88.2 | 85.3 | 77.4 | 2.8 | 1.6 | 0.3 |
| K#H4+SSLSNIFF_PB_WAKEUP=32 | 21 | 88.3 | 85.3 | 77.4 | 2.8 | 1.6 | 0.3 |
| Ol#H4 | 21 | 140.2 | 135.5 | 87.1 | 27.3 | 24.3 | 3.7 |
| OlL3#H4 | 21 | 140.1 | 135.5 | 87.1 | 27.5 | 24.4 | 3.7 |
| OlL3x730#H4 | 21 | 140.1 | 135.4 | 87.1 | 27.5 | 24.3 | 3.7 |
## DN1-20261126-try1 (n rows 168)
| leg | n | server Rss MB | server Pss MB | server Private_Dirty MB | consumer Rss MB | consumer Pss MB | consumer Private_Dirty MB |
|---|---:|---:|---:|---:|---:|---:|---:|
| B | 21 | 10.5 | 7.1 | 4.8 | nan | nan | nan |
| Dn | 21 | 20.2 | 11.9 | 4.9 | 45.5 | 42.2 | 7.6 |
| Dnext | 21 | 20.2 | 11.8 | 4.9 | 45.4 | 42.1 | 7.6 |
| Dnext+BPFTIME_CAREFUL=1 | 21 | 20.1 | 11.8 | 4.9 | 45.4 | 42.1 | 7.6 |
| Dnextpc | 21 | 20.5 | 12.0 | 4.9 | 45.6 | 42.3 | 7.6 |
| Dnextpc+BPFTIME_CAREFUL=1 | 21 | 20.4 | 11.9 | 4.9 | 45.6 | 42.3 | 7.6 |
| Dnpc | 21 | 20.6 | 11.9 | 4.9 | 45.5 | 42.2 | 7.6 |
| K+SSLSNIFF_PB_WAKEUP=32 | 21 | 10.5 | 7.0 | 4.8 | 2.6 | 1.3 | 0.3 |
## M4-20261126-try1 (n rows 126)
| leg | n | server Rss MB | server Pss MB | server Private_Dirty MB | consumer Rss MB | consumer Pss MB | consumer Private_Dirty MB |
|---|---:|---:|---:|---:|---:|---:|---:|
| B | 21 | 10.5 | 7.2 | 4.8 | nan | nan | nan |
| Dnextpc | 21 | 20.5 | 11.9 | 4.9 | 45.6 | 42.3 | 7.6 |
| Dviewpc | 21 | 20.6 | 12.0 | 4.9 | 45.5 | 42.4 | 7.6 |
| Dviewpc+BPFTIME_CAREFUL=1 | 21 | 20.5 | 11.9 | 4.9 | 45.7 | 42.4 | 7.6 |
| Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1 | 21 | 36.5 | 19.9 | 4.9 | 45.7 | 42.4 | 7.6 |
| K+SSLSNIFF_PB_WAKEUP=32 | 21 | 10.5 | 7.1 | 4.8 | 2.6 | 1.3 | 0.3 |
