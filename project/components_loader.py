import os
import SCons
from building import *

def load_components(obj, base_dir, components_dir='components'):
    """
    自动加载指定目录下的所有RT-Thread组件
    
    Args:
        env: SCons环境对象
        base_dir: 项目根目录路径
        components_dir: 存放组件的目录名(相对路径)
    """
    print("\n[RT-Thread] Auto-loading components from:", components_dir)
    
    # 计算组件目录的绝对路径
    comp_abs_path = os.path.join(base_dir, components_dir)
    
    if not os.path.exists(comp_abs_path):
        print(f"  ! Warning: Components directory not found: {comp_abs_path}")
        return
    
    # 组件计数器
    loaded_count = 0
    
    # 遍历组件目录
    for comp_name in sorted(os.listdir(comp_abs_path)):
        comp_path = os.path.join(comp_abs_path, comp_name)
        
        # 检查是否是目录且包含SConscript文件
        sconscript_path = os.path.join(comp_path, 'SConscript')
        if os.path.isdir(comp_path) and os.path.exists(sconscript_path):
            print(f"  ✓ Loading: {comp_name}")
            
            # ★ 核心：加载组件的SConscript文件
            obj.extend(SConscript(sconscript_path, variant_dir=os.path.join('components_loader', comp_name), duplicate=0))

            loaded_count += 1
        elif os.path.isdir(comp_path):
            # 存在目录但无SConscript文件，打印警告
            print(f"  ⚠ Ignored: {comp_name} (no SConscript found)")
    
    print(f"[RT-Thread] Total {loaded_count} components loaded\n")

# ★ RT-Thread兼容接口（符合RTT插件规范）★
def pre_component_loader(obj, root_directory):
    """RT-Thread标准插件初始化函数"""
    # 确保所有路径都是绝对路径
    base_dir = os.path.abspath(root_directory)
    # 调用加载函数
    load_components(obj, base_dir)
    
# 模块自检（非必须）
if __name__ == '__main__':
    print("Component Loader Module - Designed for RT-Thread")