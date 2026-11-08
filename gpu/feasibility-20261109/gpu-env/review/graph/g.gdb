set pagination off
set confirm off
set breakpoint pending on
set print thread-events off
break cuGraphAddKernelNode
break cuGraphAddKernelNode_v2
break cuGraphAddNode
break cuGraphAddNode_v2
break cuGraphKernelNodeSetParams
break cuGraphKernelNodeSetParams_v2
break cuGraphNodeSetParams
break cuGraphExecKernelNodeSetParams
break cuGraphExecKernelNodeSetParams_v2
break cuGraphExecNodeSetParams
break cuLaunchKernel
break cuGraphLaunch
run
while 1
  bt 2
  continue
end
