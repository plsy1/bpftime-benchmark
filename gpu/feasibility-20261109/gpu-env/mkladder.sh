#!/bin/bash
set -e
cd "$(dirname "$0")/ptx_ladder"
rm -f ./*.ptx
mk() { v=$1; t=$2; cat > "v${v}_${t}.ptx" <<EOF
.version $v
.target $t
.address_size 64

.visible .entry k(
	.param .u64 k_param_0
)
{
	.reg .b32 	%r<2>;
	.reg .b64 	%rd<3>;

	ld.param.u64 	%rd1, [k_param_0];
	cvta.to.global.u64 	%rd2, %rd1;
	mov.u32 	%r1, 42;
	st.global.u32 	[%rd2], %r1;
	ret;
}
EOF
}
for vt in 5.0:sm_60 7.0:sm_60 8.1:sm_60 5.0:sm_61 8.1:sm_61 6.0:sm_70 8.1:sm_70 6.1:sm_72 8.1:sm_72 6.3:sm_75 8.1:sm_75 9.2:sm_75 7.0:sm_80 9.2:sm_80 7.1:sm_86 9.2:sm_86 5.0:sm_87 7.0:sm_87 7.3:sm_87 7.4:sm_87 7.5:sm_87 7.8:sm_87 8.1:sm_87 8.5:sm_87 9.0:sm_87 9.1:sm_87 9.2:sm_87 9.3:sm_87 9.4:sm_87 9.5:sm_87 10.0:sm_87 9.2:sm_88 9.2:sm_89 9.2:sm_90; do
  mk "${vt%%:*}" "${vt##*:}"
done
ls
