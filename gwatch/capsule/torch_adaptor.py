from typing import Any, List

import torch
import torch.nn as nn
from torch.utils._python_dispatch import TorchDispatchMode

import gwatch.libpygwatch as pygwatch
from gwatch.capsule.event import GWEvent

# take these refs:
# https://dev-discuss.pytorch.org/t/what-and-why-is-torch-dispatch/557
# https://dev-discuss.pytorch.org/t/the-ideal-pytorch-flop-counter-with-torch-dispatch/505
# https://pastebin.com/V3wATa7w
# https://pastebin.com/AkvAyJBw
# https://dev-discuss.pytorch.org/t/torchdispatchmode-for-debugging-testing-and-more/717
class GWModelAnlyser(TorchDispatchMode):
    def __init__(self, module : nn.Module):
        super().__init__()
        self._module = module
        self._module_app_range_event : List[GWEvent] = []

        # we maintain this list to prevent the app range event from being garbage collected
        self._event_keepalive : List[GWEvent] = []
        
        self.__parse_module(module)


    def __torch_dispatch__(self, func, types, args=(), kwargs=None):
        """
        op-level hijack
        """
        input_tensor_info : List = []
        output_tensor_info : List = []
        kwargs = kwargs if kwargs else {}

        # create new app range event
        app_range_event : GWEvent = GWEvent(func.__name__)
        pygwatch.push_event(app_range_event._C_instance)
        self._event_keepalive.append(app_range_event)

        # record input tensor info
        input_tensor_info = GWModelAnlyser.__collect_tensor_info(args)
        input_tensor_info += GWModelAnlyser.__collect_tensor_info(kwargs.values())
        app_range_event.set_metadata("input_tensor_info", input_tensor_info)

        # execute the operator
        app_range_event.record_tick("begin")
        out = func(*args, **kwargs)
        app_range_event.record_tick("end")

        # record output tensor info
        output_tensor_info = GWModelAnlyser.__collect_tensor_info(out)
        app_range_event.set_metadata("output_tensor_info", output_tensor_info)

        # archive the app range event
        app_range_event.archive()

        return out


    def __parse_module(self, module : nn.Module):
        """
        module-level hijack
        """
        module.register_forward_pre_hook(GWModelAnlyser.__pre_nn_module_forward(self))
        module.register_forward_hook(GWModelAnlyser.__post_nn_module_forward(self))
        # TODO(zhuobin, shen): register_backward_pre_hook, register_backward_hook
        # link: 
        # could be a bug: keyword issue :o
        for child in module.children():
            self.__parse_module(child)


    @staticmethod
    def __pre_nn_module_forward(self : 'GWModelAnlyser'):
        def _func(module : nn.Module, input : Any):
            # create new app range event
            app_range_event : GWEvent = GWEvent(module.__class__.__name__)
            pygwatch.push_event(app_range_event._C_instance)

            # push the event to the trace stack
            pygwatch.push_parent_event(app_range_event._C_instance)

            self._event_keepalive.append(app_range_event)

            # record input tensor info
            input_tensor_info = GWModelAnlyser.__collect_tensor_info(input)
            app_range_event.set_metadata("input_tensor_info", input_tensor_info)

            # save the app range event to stack for later used
            self._module_app_range_event.append(app_range_event)

            # start ticking
            app_range_event.record_tick("begin")
            
        return _func


    @staticmethod
    def __post_nn_module_forward(self : 'GWModelAnlyser'):
        def _func(module : nn.Module, input : Any, output : Any):
            # obtain the app range event from stack
            try:
                app_range_event = self._module_app_range_event.pop()
            except:
                raise RuntimeError("no app range event found")

            # end ticking
            app_range_event.record_tick("end")

            # record output tensor info
            output_tensor_info = GWModelAnlyser.__collect_tensor_info(output)
            app_range_event.set_metadata("output_tensor_info", output_tensor_info)
    
            # pop the event from the trace stack
            pygwatch.pop_parent_event()

            # archive the app range event
            app_range_event.archive()

        return _func


    @staticmethod
    def __collect_tensor_info(tensors : Any):
        info : List = []
        def _recursive_collect(elem):
            if isinstance(elem, torch.Tensor):
                # TODO(zhuobin, shen): add tensor stride
                info.append({
                    "shape": str(list(elem.shape)),
                    "dtype": str(elem.dtype).split(".")[-1],
                    "ptr": str(elem.data_ptr()),
                    "device": str(elem.device)
                })
            elif isinstance(elem, (list, tuple)):
                for e in elem:
                    _recursive_collect(e)
            elif isinstance(elem, dict):
                for v in elem.values():
                    _recursive_collect(v)
        _recursive_collect(tensors)
        return info


__all__ = [ "GWModelAnlyser" ]
