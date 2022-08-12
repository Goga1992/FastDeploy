# FastDeploy安装

目前已提供Linux、Windows上的Python Wheel安装包，开发者根据自身需求下载后使用如下命令安装即可，如

```
python -m pip install fastdeploy_python-0.2.0-cp36-cp36m-manylinux1_x86_64.whl
```

注意环境中不要重复安装`fastdeploy-python`和`fastdeploy-gpu-python`，在安装前，例如已安装cpu版本的`fastdeploy-python`后，如想重新安装gpu版本的`fastdeploy-gpu-python`，请先执行`pip uninstall fastdeploy-python`卸载已有版本

## Linux 

| CPU安装包                                                       | Python环境 | 支持硬件 |
| ------------------------------------------------------------ | ---------- | -------- |
| [fastdeploy_python-0.2.0-cp36-cp36m-manylinux1_x86_64.whl](https://bj.bcebos.com/paddlehub/fastdeploy/wheels/fastdeploy_python-0.2.0-cp36-cp36m-manylinux1_x86_64.whl) | 3.6        | CPU      |
| [fastdeploy_python-0.2.0-cp37-cp37m-manylinux1_x86_64.whl](https://bj.bcebos.com/paddlehub/fastdeploy/wheels/fastdeploy_python-0.2.0-cp37-cp37m-manylinux1_x86_64.whl) | 3.7        | CPU      |
| [fastdeploy_python-0.2.0-cp38-cp38-manylinux1_x86_64.whl](https://bj.bcebos.com/paddlehub/fastdeploy/wheels/fastdeploy_python-0.2.0-cp38-cp38-manylinux1_x86_64.whl) | 3.8        | CPU      |
| [fastdeploy_python-0.2.0-cp39-cp39-manylinux1_x86_64.whl](https://bj.bcebos.com/paddlehub/fastdeploy/wheels/fastdeploy_python-0.2.0-cp39-cp39-manylinux1_x86_64.whl) | 3.9        | CPU      |

| GPU安装包                                                       | Python环境 | 支持硬件 |
| ------------------------------------------------------------ | ---------- | -------- |
| [fastdeploy_gpu_python-0.2.0-cp36-cp36m-manylinux1_x86_64.whl](https://bj.bcebos.com/paddlehub/fastdeploy/wheels/fastdeploy_gpu_python-0.2.0-cp36-cp36m-manylinux1_x86_64.whl) | 3.6        | CPU/GPU(CUDA11.2 CUDNN8)     |
| [fastdeploy_gpu_python-0.2.0-cp37-cp37m-manylinux1_x86_64.whl](https://bj.bcebos.com/paddlehub/fastdeploy/wheels/fastdeploy_gpu_python-0.2.0-cp37-cp37m-manylinux1_x86_64.whl) | 3.7        | CPU/GPU(CUDA11.2 CUDNN8)       |
| [fastdeploy_gpu_python-0.2.0-cp38-cp38-manylinux1_x86_64.whl](https://bj.bcebos.com/paddlehub/fastdeploy/wheels/fastdeploy_gpu_python-0.2.0-cp38-cp38-manylinux1_x86_64.whl) | 3.8        | CPU/GPU(CUDA11.2 CUDNN8)       |
| [fastdeploy_gpu_python-0.2.0-cp39-cp39-manylinux1_x86_64.whl](https://bj.bcebos.com/paddlehub/fastdeploy/wheels/fastdeploy_gpu_python-0.2.0-cp39-cp39-manylinux1_x86_64.whl) | 3.9        | CPU/GPU(CUDA11.2 CUDNN8)       |