from typing import List

import torch
import triton
import triton.language as tl

import gwatch.cuda.assemble as gw_assemble
import gwatch.cuda.profile as gw_profile


@triton.jit
def mul_kernel(
    x_ptr,
    y_ptr,
    output_ptr,
    n_elements,
    BLOCK_SIZE: tl.constexpr,
):
    pid = tl.program_id(axis=0)

    block_start = pid * BLOCK_SIZE
    offsets = block_start + tl.arange(0, BLOCK_SIZE)

    mask = offsets < n_elements

    x = tl.load(x_ptr + offsets, mask=mask)
    y = tl.load(y_ptr + offsets, mask=mask)

    output = x * y
    tl.store(output_ptr + offsets, output, mask=mask)


def triton_mul(x: torch.Tensor, y: torch.Tensor):
    assert x.is_cuda and y.is_cuda
    assert x.shape == y.shape

    n_elements = x.numel()
    output = torch.empty_like(x)

    BLOCK_SIZE = 1024
    grid = lambda meta: (triton.cdiv(n_elements, meta['BLOCK_SIZE']), )
    mul_kernel[grid](x, y, output, n_elements, BLOCK_SIZE=BLOCK_SIZE)

    return output


if __name__ == "__main__":
    torch.manual_seed(0)
    list_kernel : List[gw_assemble.KernelCUDA] = []

    # create profiler
    pcontext = gw_profile.ProfileContext()
    profiler = pcontext.create_profiler(device_id = 0, metric_names = [], mode = "pc")

    size = 98432
    x = torch.randn(size, device='cuda')
    y = torch.randn(size, device='cuda')

    # pc sampling
    with profiler.run() as P:
        output_triton = triton_mul(x, y)
        torch.cuda.synchronize()
    print(P.metrics)

    output_torch = x * y
    assert torch.allclose(output_triton, output_torch), "Triton output does not match PyTorch output"
