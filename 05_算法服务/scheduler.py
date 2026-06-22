"""
调度优化微服务 — Flask :5000
核心算法：IMOGJO（改进多目标金豺优化），改自毕业论文2.0.py

做了什么：
- 用元启发式算法同时优化两个目标：完工时间最短 + 能耗最低
- 从SpringBoot拿设备实时状态，故障机器自动剔除、预警机器降速1.2倍
- 输出Pareto前沿 + 甘特图数据 + 无约束对比（看故障劣化了多少）
- 支持工期/能耗权重调节，滑块拖动实时重新选优

算法思路（简版）：
1. 种群用实数编码，前半段OS（工序排序）+ 后半段MS（机器选择）
2. 解码后用非支配排序分Pareto层级，拥挤距离保持多样性
3. 模拟金豺狩猎行为更新种群（雄豺+雌豺引导，Levy飞行探索，VNS局部搜索）
4. 迭代结束后输出Pareto前沿，前端按权重选最优方案

参考：Chopra N. et al., Golden Jackal Optimization, Expert Systems with Applications, 2022
"""

import numpy as np
import math
import os
import sys
import time
import socket
import subprocess
import requests
from flask import Flask, request, jsonify
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

# ==================== 设备状态融合层 ====================

BACKEND_URL = "http://localhost:8081/api/devices"

DEVICE_TO_MACHINE = {1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 5}
DEVICE_NAMES = {1: "数控铣床", 2: "数控车床", 3: "磨床", 4: "钻床", 5: "冲压机", 6: "注塑机"}


def fetch_device_status():
    """从SpringBoot后端获取6台设备实时状态，失败返回None"""
    try:
        resp = requests.get(BACKEND_URL, timeout=5)
        if resp.status_code == 200:
            return resp.json()
    except requests.RequestException:
        pass
    return None


def apply_device_constraints(case, devices):
    """
    将设备状态映射为调度约束（三层极简逻辑）：
    - status=0(正常) → 无影响
    - status=1(预警) → 加工时间×1.2轻微降速
    - status=2(故障) → 加工时间置0，从可选机器集剔除
    """
    n_machines = case['n_machines']
    constrained = []
    dev_map = {d['id']: d for d in devices} if devices else {}

    for dev_id, mach_id in DEVICE_TO_MACHINE.items():
        if mach_id >= n_machines:
            continue
        dev = dev_map.get(dev_id, {})
        status = dev.get('dataStatus', 0)

        if status == 2:
            for job in range(case['n_jobs']):
                for op in range(case['jobs_n_ops'][job]):
                    case['processing_time'][job][op][mach_id] = 0
            constrained.append({'machine': mach_id, 'name': DEVICE_NAMES[dev_id], 'status': 'fault', 'action': '已剔除'})

        elif status == 1:
            for job in range(case['n_jobs']):
                for op in range(case['jobs_n_ops'][job]):
                    pt = case['processing_time'][job][op][mach_id]
                    if pt != 0:
                        case['processing_time'][job][op][mach_id] = round(pt * 1.2, 2)
            constrained.append({'machine': mach_id, 'name': DEVICE_NAMES[dev_id], 'status': 'warning', 'action': '降速x1.2'})

    return case, constrained


def build_case_from_devices(devices, jobs_data=None):
    """从设备数据构建算法案例，融合论文设备参数"""
    if jobs_data is None:
        jobs_data = DEFAULT_JOBS
    if not isinstance(jobs_data, dict):
        jobs_data = DEFAULT_JOBS
    if devices:
        n_machines = min(len(devices), 6)
    else:
        n_machines = jobs_data.get('n_machines', 4)

    # 扩展加工时间矩阵到目标机器数（多余机器列置0，表示不可用）
    src_machines = len(jobs_data['processing_time'][0][0])
    proc = []
    for job in jobs_data['processing_time']:
        job_ops = []
        for op in job:
            if n_machines > src_machines:
                job_ops.append(op + [0] * (n_machines - src_machines))
            else:
                job_ops.append(op[:n_machines])
        proc.append(job_ops)

    case = {
        'n_jobs': jobs_data['n_jobs'],
        'n_machines': n_machines,
        'jobs_n_ops': jobs_data['jobs_n_ops'][:],
        'processing_time': proc,
        'load_power': DEFAULT_POWER['load_power'][:n_machines],
        'idle_power': DEFAULT_POWER['idle_power'][:n_machines],
        'switch_energy': DEFAULT_POWER['switch_energy'][:n_machines],
        'deterioration_rate': DEFAULT_POWER['deterioration_rate'][:n_machines],
        'deterioration_low': DEFAULT_POWER['deterioration_low'][:n_machines],
        'deterioration_high': DEFAULT_POWER['deterioration_high'][:n_machines],
    }
    if devices:
        case, constrained = apply_device_constraints(case, devices)
    else:
        constrained = []
    return case, constrained


# 默认案例1（4工件×4机器），与2.0.py完全一致
DEFAULT_JOBS = {
    'n_jobs': 4, 'n_machines': 4,
    'jobs_n_ops': [2, 3, 2, 2],
    'processing_time': [
        [[1, 8, 7, 5], [3, 2, 6, 3]],
        [[7, 6, 5, 2], [3, 2, 8, 5], [4, 7, 3, 6]],
        [[1, 5, 9, 5], [8, 3, 4, 7]],
        [[6, 5, 4, 5], [2, 7, 10, 6]]
    ]
}

DEFAULT_POWER = {
    'load_power': [3.4, 4.5, 4.2, 3.8, 4.0, 3.9],
    'idle_power': [1.2, 1.7, 1.5, 1.6, 1.3, 1.4],
    'switch_energy': [7.3, 9.5, 6.4, 7.7, 8.0, 7.0],
    'deterioration_rate': [0.3, 0.4, 0.2, 0.5, 0.3, 0.35],
    'deterioration_low': [4, 5, 3, 4, 4, 4],
    'deterioration_high': [10, 12, 9, 10, 10, 10]
}

# ==================== 从2.0.py提取的核心算法 ====================

def total_operations(case):
    return sum(case['jobs_n_ops'])


def machine_options(case, job, op):
    return [m for m in range(case['n_machines']) if case['processing_time'][job][op][m] != 0]


def actual_processing_time(case, machine, base_time, accum_time):
    t_low = case['deterioration_low'][machine]
    t_high = case['deterioration_high'][machine]
    delta = case['deterioration_rate'][machine]
    if accum_time < t_low:
        return base_time
    elif accum_time <= t_high:
        return base_time + delta * (accum_time - t_low)
    else:
        return base_time + delta * (t_high - t_low)


def decode_schedule(case, os_list, ms_list, return_tasks=False):
    n_jobs = case['n_jobs']
    n_machines = case['n_machines']
    machine_tasks = [[] for _ in range(n_machines)]
    job_ready = [0.0] * n_jobs
    machine_accum = [0.0] * n_machines
    machine_on = [False] * n_machines
    total_energy = 0.0
    makespan = 0.0
    ms_dict = {}
    idx = 0
    for job in range(n_jobs):
        for op in range(case['jobs_n_ops'][job]):
            ms_dict[(job, op)] = ms_list[idx]
            idx += 1
    job_progress = [0] * n_jobs
    for job_id in os_list:
        job = job_id - 1
        op = job_progress[job]
        m = ms_dict[(job, op)]
        job_progress[job] += 1
        bt = case['processing_time'][job][op][m]
        act = actual_processing_time(case, m, bt, machine_accum[m])
        if machine_tasks[m]:
            tasks = sorted(machine_tasks[m], key=lambda x: x[0])
            last_end = tasks[-1][1]
        else:
            tasks = []
            last_end = 0.0
        inserted = False
        best_start = max(job_ready[job], last_end)
        prev_end = 0.0
        for (t_start, t_end, _, _) in tasks:
            gap_start = max(prev_end, job_ready[job])
            gap_end = t_start
            if gap_end - gap_start >= act:
                best_start = gap_start
                inserted = True
                break
            prev_end = max(prev_end, t_end)
        if not inserted:
            if tasks:
                best_start = max(job_ready[job], tasks[-1][1])
            else:
                best_start = max(job_ready[job], 0.0)
        if tasks:
            idle = best_start - tasks[-1][1]
        else:
            idle = best_start
        if idle > 0:
            if machine_on[m]:
                eng_idle = idle * case['idle_power'][m]
                if eng_idle > case['switch_energy'][m]:
                    total_energy += case['switch_energy'][m]
                    machine_on[m] = False
                else:
                    total_energy += eng_idle
        if not machine_on[m]:
            total_energy += case['switch_energy'][m]
            machine_on[m] = True
        total_energy += act * case['load_power'][m]
        machine_tasks[m].append((best_start, best_start + act, job, op))
        job_ready[job] = best_start + act
        machine_accum[m] += act
        makespan = max(makespan, best_start + act)
    if return_tasks:
        return makespan, total_energy, machine_tasks
    return makespan, total_energy


def encode_decode(case, position):
    total_ops = total_operations(case)
    n_jobs = case['n_jobs']
    pos_os = position[:total_ops]
    idx_sorted = np.argsort(pos_os)
    job_seq = []
    for job in range(n_jobs):
        for _ in range(case['jobs_n_ops'][job]):
            job_seq.append(job + 1)
    os_seq = [job_seq[i] for i in idx_sorted]
    pos_ms = position[total_ops:]
    ms_seq = []
    op_idx = 0
    for job in range(n_jobs):
        for op in range(case['jobs_n_ops'][job]):
            opts = machine_options(case, job, op)
            if len(opts) == 0:
                ms_seq.append(0)
            else:
                s = len(opts)
                if s == 1:
                    ms_seq.append(opts[0])
                else:
                    val = pos_ms[op_idx]
                    idx_m = int(round(((val + n_jobs) * (s - 1) / (2 * n_jobs) + 1))) - 1
                    idx_m = max(0, min(s - 1, idx_m))
                    ms_seq.append(opts[idx_m])
            op_idx += 1
    return os_seq, ms_seq


def evaluate(case, os_seq, ms_seq):
    return decode_schedule(case, os_seq, ms_seq)


def init_population_glr(case, pop_size, lb, ub, total_ops, dim):
    n_jobs = case['n_jobs']
    pop = np.zeros((pop_size, dim))
    n_gs = int(pop_size * 0.6)
    n_ls = int(pop_size * 0.3)
    n_rs = pop_size - n_gs - n_ls
    idx = 0
    for _ in range(n_gs):
        os_part = np.random.uniform(lb, ub, total_ops)
        mach_load = [0.0] * case['n_machines']
        ms_list = []
        for job in range(n_jobs):
            for op in range(case['jobs_n_ops'][job]):
                opts = machine_options(case, job, op)
                if not opts:
                    best_m = 0
                else:
                    best_m = opts[0]
                    best_load = case['processing_time'][job][op][best_m] + mach_load[best_m]
                    for m in opts[1:]:
                        load = case['processing_time'][job][op][m] + mach_load[m]
                        if load < best_load:
                            best_load = load
                            best_m = m
                ms_list.append(best_m)
                mach_load[best_m] += case['processing_time'][job][op][best_m]
        ms_cont = []
        op_cnt = 0
        for job in range(n_jobs):
            for op in range(case['jobs_n_ops'][job]):
                chosen = ms_list[op_cnt]
                opts = machine_options(case, job, op)
                if not opts:
                    ms_cont.append(0.0)
                else:
                    s = len(opts)
                    if s == 1:
                        ms_cont.append(0.0)
                    else:
                        u = opts.index(chosen) + 1
                        val = ((u - 1) * 2 * n_jobs / (s - 1)) - n_jobs
                        ms_cont.append(val)
                op_cnt += 1
        pop[idx] = np.hstack([os_part, np.array(ms_cont)])
        idx += 1
    for _ in range(n_ls):
        os_part = np.random.uniform(lb, ub, total_ops)
        ms_cont = []
        for job in range(n_jobs):
            for op in range(case['jobs_n_ops'][job]):
                opts = machine_options(case, job, op)
                if not opts:
                    ms_cont.append(0.0)
                else:
                    if len(opts) == 1:
                        ms_cont.append(0.0)
                    else:
                        best = min(opts, key=lambda m: case['processing_time'][job][op][m])
                        u = opts.index(best) + 1
                        s = len(opts)
                        val = ((u - 1) * 2 * n_jobs / (s - 1)) - n_jobs
                        ms_cont.append(val)
        pop[idx] = np.hstack([os_part, np.array(ms_cont)])
        idx += 1
    for _ in range(n_rs):
        pop[idx] = np.random.uniform(lb, ub, dim)
        idx += 1
    return pop


def levy_flight(dim, beta=1.5):
    sigma = (math.gamma(1 + beta) * math.sin(math.pi * beta / 2) /
             (math.gamma((1 + beta) / 2) * beta * 2 ** ((beta - 1) / 2))) ** (1 / beta)
    u = np.random.normal(0, sigma, dim)
    v = np.random.normal(0, 1, dim)
    return 0.05 * u / (np.abs(v) ** (1 / beta))


def energy_decay(t, max_iter):
    return 1.5 * (1 - t / max_iter) ** 3


def adaptive_mutation(pos, t, max_iter):
    rate = 0.2 * (1 - t / max_iter)
    mask = np.random.rand(len(pos)) < rate
    noise = np.random.normal(0, 0.1, len(pos))
    pos = pos.copy()
    pos[mask] += noise[mask]
    return pos


def validate_os(case, os_seq):
    count = [0] * case['n_jobs']
    for job_id in os_seq:
        count[job_id - 1] += 1
    return count == case['jobs_n_ops']


def vns(case, os_seq, ms_seq, max_trials=3):
    best_os, best_ms = os_seq.copy(), ms_seq.copy()
    best_fit = evaluate(case, best_os, best_ms)
    total_ops = len(os_seq)
    for _ in range(max_trials):
        i, j = np.random.choice(total_ops, 2, replace=False)
        new_os = best_os.copy()
        elem = new_os.pop(j)
        new_os.insert(i, elem)
        if validate_os(case, new_os):
            fit = evaluate(case, new_os, best_ms)
            if dominates(fit, best_fit):
                best_os, best_fit = new_os, fit
                break
    for _ in range(max_trials):
        i, j = np.random.choice(total_ops, 2, replace=False)
        new_os = best_os.copy()
        new_os[i], new_os[j] = new_os[j], new_os[i]
        if validate_os(case, new_os):
            fit = evaluate(case, new_os, best_ms)
            if dominates(fit, best_fit):
                best_os, best_fit = new_os, fit
                break
    for _ in range(max_trials):
        new_ms = best_ms.copy()
        idx = np.random.randint(total_ops)
        cum = 0
        for job in range(case['n_jobs']):
            next_cum = cum + case['jobs_n_ops'][job]
            if idx < next_cum:
                op_local = idx - cum
                opts = machine_options(case, job, op_local)
                if len(opts) > 1:
                    times = [case['processing_time'][job][op_local][m] for m in opts]
                    new_ms[idx] = opts[np.argmin(times)]
                break
            cum = next_cum
        fit = evaluate(case, best_os, new_ms)
        if dominates(fit, best_fit):
            best_ms, best_fit = new_ms, fit
            break
    return best_os, best_ms, best_fit


def dominates(f1, f2):
    return (f1[0] <= f2[0] and f1[1] <= f2[1]) and (f1[0] < f2[0] or f1[1] < f2[1])


def non_dominated_sort(fits):
    n = len(fits)
    dom_counts = [0] * n
    dom_lists = [[] for _ in range(n)]
    for i in range(n):
        for j in range(n):
            if i == j: continue
            if dominates(fits[i], fits[j]):
                dom_lists[i].append(j)
            elif dominates(fits[j], fits[i]):
                dom_counts[i] += 1
    ranks = [0] * n
    front = [i for i in range(n) if dom_counts[i] == 0]
    r = 0
    while front:
        for i in front:
            ranks[i] = r
        next_front = []
        for i in front:
            for j in dom_lists[i]:
                dom_counts[j] -= 1
                if dom_counts[j] == 0:
                    next_front.append(j)
        front = next_front
        r += 1
    return ranks


def crowding_distance(fits, ranks):
    n = len(fits)
    dist = [0.0] * n
    max_rank = max(ranks) if ranks else 0
    for r in range(max_rank + 1):
        idx = [i for i in range(n) if ranks[i] == r]
        if len(idx) <= 2:
            for i in idx: dist[i] = float('inf')
            continue
        idx_s = sorted(idx, key=lambda i: fits[i][0])
        fmin, fmax = fits[idx_s[0]][0], fits[idx_s[-1]][0]
        if fmax - fmin > 1e-6:
            for i in range(1, len(idx_s) - 1):
                dist[idx_s[i]] += (fits[idx_s[i + 1]][0] - fits[idx_s[i - 1]][0]) / (fmax - fmin)
        dist[idx_s[0]] = float('inf')
        dist[idx_s[-1]] = float('inf')
        idx_s = sorted(idx, key=lambda i: fits[i][1])
        fmin, fmax = fits[idx_s[0]][1], fits[idx_s[-1]][1]
        if fmax - fmin > 1e-6:
            for i in range(1, len(idx_s) - 1):
                dist[idx_s[i]] += (fits[idx_s[i + 1]][1] - fits[idx_s[i - 1]][1]) / (fmax - fmin)
    return dist


def imogjo(case, pop_size=30, max_iter=200):
    total_ops = total_operations(case)
    n_jobs = case['n_jobs']
    dim = total_ops * 2
    lb, ub = -n_jobs, n_jobs
    pop = init_population_glr(case, pop_size, lb, ub, total_ops, dim)
    solutions = [None] * pop_size
    fits = [None] * pop_size
    for i in range(pop_size):
        os_seq, ms_seq = encode_decode(case, pop[i])
        c, e = evaluate(case, os_seq, ms_seq)
        solutions[i] = (os_seq, ms_seq)
        fits[i] = (c, e)
    archive = []
    for t in range(max_iter):
        ranks = non_dominated_sort(fits)
        crowds = crowding_distance(fits, ranks)
        order = sorted(range(pop_size), key=lambda i: (ranks[i], -crowds[i]))
        male_idx, female_idx = order[0], order[1] if len(order) > 1 else order[0]
        Y_male = pop[male_idx].copy()
        Y_female = pop[female_idx].copy()
        for i in range(pop_size):
            E1 = energy_decay(t, max_iter)
            E0 = 2 * np.random.rand() - 1
            E = E1 * E0
            rl = levy_flight(dim)
            if abs(E) >= 1:
                Y1 = Y_male - E * np.abs(Y_male - rl * pop[i])
                Y2 = Y_female - E * np.abs(Y_female - rl * pop[i])
            else:
                Y1 = Y_male - E * np.abs(rl * Y_male - pop[i])
                Y2 = Y_female - E * np.abs(rl * Y_female - rl * pop[i])
            new_pos = (Y1 + Y2) / 2
            new_pos = np.clip(new_pos, lb, ub)
            new_pos = adaptive_mutation(new_pos, t, max_iter)
            new_pos = np.clip(new_pos, lb, ub)
            os_seq, ms_seq = encode_decode(case, new_pos)
            c, e = evaluate(case, os_seq, ms_seq)
            new_fit = (c, e)
            if dominates(new_fit, fits[i]):
                pop[i] = new_pos
                fits[i] = new_fit
                solutions[i] = (os_seq, ms_seq)
        for idx in order[:3]:
            os_seq, ms_seq = solutions[idx]
            os2, ms2, fit2 = vns(case, os_seq, ms_seq, 3)
            if dominates(fit2, fits[idx]):
                solutions[idx] = (os2, ms2)
                fits[idx] = fit2
        for i in range(pop_size):
            if ranks[i] == 0:
                archive.append((fits[i], (solutions[i][0], solutions[i][1])))
        unique = {}
        for f, s in archive:
            key = (round(f[0], 2), round(f[1], 2))
            if key not in unique:
                unique[key] = (f, s)
        archive = list(unique.values())[:pop_size]
    if archive:
        pareto_fits = [x[0] for x in archive]
        pareto_sols = [x[1] for x in archive]
    else:
        ranks = non_dominated_sort(fits)
        pareto_fits = [fits[i] for i in range(pop_size) if ranks[i] == 0]
        pareto_sols = [solutions[i] for i in range(pop_size) if ranks[i] == 0]
    return pareto_sols, pareto_fits


# ==================== Gantt数据转换 ====================

def build_gantt_data(case, os_seq, ms_seq):
    makespan, energy, tasks = decode_schedule(case, os_seq, ms_seq, return_tasks=True)
    gantt_tasks = []
    for m in range(case['n_machines']):
        for (start, end, job, op) in tasks[m]:
            gantt_tasks.append({
                'machine': m,
                'machineName': DEVICE_NAMES.get(m + 1, f'设备{m + 1}'),
                'job': job,
                'op': op,
                'start': round(start, 2),
                'end': round(end, 2),
                'duration': round(end - start, 2)
            })
    return gantt_tasks, makespan, energy


# ==================== Flask API ====================

@app.route('/api/optimize', methods=['POST'])
def optimize():
    data = request.get_json(silent=True) or {}
    use_device = data.get('use_device_status', True)
    jobs_data = data.get('jobs_data', None)
    pop_size = data.get('pop_size', 30)
    max_iter = data.get('max_iter', 200)
    weight_makespan = data.get('weight_makespan', 0.5)
    weight_energy = data.get('weight_energy', 0.5)

    devices = None
    device_status = None
    if use_device:
        dev_data = fetch_device_status()
        if dev_data:
            devices = dev_data.get('devices', [])
            device_status = {
                'total': dev_data.get('totalDevices', 0),
                'normal': dev_data.get('normalCount', 0),
                'warning': dev_data.get('warningCount', 0),
                'fault': dev_data.get('faultCount', 0),
                'connected': True
            }

    if not device_status:
        device_status = {
            'total': 0, 'normal': 0, 'warning': 0, 'fault': 0,
            'connected': False, 'note': '后端未连接，使用默认参数'
        }

    case, constrained = build_case_from_devices(devices, jobs_data)

    try:
        pareto_sols, pareto_fits = imogjo(case, pop_size=pop_size, max_iter=max_iter)
    except Exception as e:
        return jsonify({'error': f'优化计算失败: {str(e)}'}), 500

    if not pareto_sols:
        return jsonify({'error': '无可选机器——可能全部设备处于故障状态'}), 400

    # Weighted selection from Pareto front
    ms_vals = [f[0] for f in pareto_fits]
    en_vals = [f[1] for f in pareto_fits]
    ms_min, ms_max = min(ms_vals), max(ms_vals)
    en_min, en_max = min(en_vals), max(en_vals)

    best_idx = 0
    best_score = float('inf')
    for i, f in enumerate(pareto_fits):
        ms_norm = (f[0] - ms_min) / (ms_max - ms_min) if ms_max > ms_min else 0
        en_norm = (f[1] - en_min) / (en_max - en_min) if en_max > en_min else 0
        score = weight_makespan * ms_norm + weight_energy * en_norm
        if score < best_score:
            best_score = score
            best_idx = i

    os_seq, ms_seq = pareto_sols[best_idx]
    gantt_data, makespan, energy = build_gantt_data(case, os_seq, ms_seq)

    pareto_front = [{'makespan': round(f[0], 2), 'energy': round(f[1], 2)} for f in pareto_fits]
    schedule = [{'os_seq': sol[0], 'ms_seq': sol[1]} for sol in pareto_sols]

    # 无约束对比：纯算法不融合设备状态
    unconstrained = {'makespan': None, 'energy': None, 'improvement_makespan': None, 'improvement_energy': None}
    if use_device and devices and constrained:
        try:
            case_unc, _ = build_case_from_devices(None, jobs_data)
            ps_unc, pf_unc = imogjo(case_unc, pop_size=pop_size, max_iter=max_iter)
            if ps_unc:
                best_unc = pf_unc[0]
                for f in pf_unc:
                    if f[0] < best_unc[0]:
                        best_unc = f
                unconstrained['makespan'] = round(best_unc[0], 2)
                unconstrained['energy'] = round(best_unc[1], 2)
                unconstrained['improvement_makespan'] = round((best_unc[0] - makespan) / best_unc[0] * 100, 1) if best_unc[0] else 0
                unconstrained['improvement_energy'] = round((best_unc[1] - energy) / best_unc[1] * 100, 1) if best_unc[1] else 0
        except Exception:
            pass

    return jsonify({
        'schedule': schedule,
        'pareto_front': pareto_front,
        'gantt_data': {'machines': list(DEVICE_NAMES.values())[:case['n_machines']], 'tasks': gantt_data},
        'device_status': device_status,
        'constrained_machines': constrained,
        'makespan': round(makespan, 2),
        'energy': round(energy, 2),
        'unconstrained': unconstrained,
        'algorithm': 'IMOGJO',
        'params': {'pop_size': pop_size, 'max_iter': max_iter, 'n_jobs': case['n_jobs'], 'n_machines': case['n_machines'], 'weight_makespan': weight_makespan, 'weight_energy': weight_energy}
    })


@app.route('/api/health', methods=['GET'])
def health():
    return jsonify({'status': 'ok', 'algorithm': 'IMOGJO'})


# ==================== System Management API ====================

def _check_port(port):
    """Check if port is listening via netstat or socket fallback"""
    try:
        r = subprocess.run(['netstat', '-ano'], capture_output=True, text=True, timeout=5)
        for line in r.stdout.split('\n'):
            if f':{port}' in line and 'LISTENING' in line:
                return True
    except Exception:
        pass
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(1)
        r = s.connect_ex(('127.0.0.1', port))
        s.close()
        return r == 0
    except Exception:
        pass
    return False


def _get_pid(port):
    """Get PID of process on a port"""
    try:
        r = subprocess.run(['netstat', '-ano'], capture_output=True, text=True, timeout=5)
        for line in r.stdout.split('\n'):
            if f':{port}' in line and 'LISTENING' in line:
                parts = line.strip().split()
                return parts[-1]
    except Exception:
        pass
    return None


@app.route('/api/system/status', methods=['GET'])
def system_status():
    services = {
        'spring': {'name': 'SpringBoot', 'port': 8081, 'running': _check_port(8081), 'type': 'process'},
        'flask': {'name': 'Flask IMOGJO', 'port': 5000, 'running': True, 'pid': str(os.getpid()), 'type': 'process'},
        'hmi': {'name': 'HMI WinForm', 'port': None, 'running': False, 'type': 'manual'},
    }
    for key in services:
        svc = services[key]
        if svc.get('port') and svc['running'] and key != 'flask':
            svc['pid'] = _get_pid(svc['port'])

    return jsonify({
        'services': services,
        'total': len(services),
        'running': sum(1 for s in services.values() if s['running']),
    })


def _kill_port(port):
    """Kill process on a port"""
    if port == 5000:  # Can't kill self
        return False, 'Cannot kill Flask (self)'
    pid = _get_pid(port)
    if pid:
        try:
            subprocess.run(['taskkill', '/PID', pid, '/F'], capture_output=True, timeout=10)
            time.sleep(0.3)
            if not _check_port(port):
                return True, f'Killed PID {pid} on port {port}'
            return False, f'PID {pid} may still be alive'
        except Exception as e:
            return False, str(e)
    return True, 'No process found'


@app.route('/api/system/stop/<name>', methods=['POST'])
def system_stop(name):
    mapping = {
        'spring': 8081,
        'flask': 5000,
        'hmi': None,
    }
    if name not in mapping:
        return jsonify({'ok': False, 'error': f'Unknown service: {name}'}), 404


    if name == 'flask':
        return jsonify({'ok': True, 'message': 'Flask shutdown initiated', 'note': 'Process will exit'})
    if name == 'hmi':
        try:
            subprocess.run(['taskkill', '/IM', 'dotnet.exe', '/F'], capture_output=True, timeout=5)
            return jsonify({'ok': True, 'message': 'HMI stopped'})
        except Exception as e:
            return jsonify({'ok': False, 'error': str(e)})

    port = mapping[name]
    ok, msg = _kill_port(port)
    return jsonify({'ok': ok, 'message': msg})


@app.route('/api/system/start/<name>', methods=['POST'])
def system_start(name):
    if name == 'flask':
        return jsonify({'ok': True, 'message': 'Flask already running (self)'})
    if name == 'spring':
        # Can't start SpringBoot from Flask easily; report status
        running = _check_port(8081)
        return jsonify({'ok': running, 'message': 'SpringBoot already running' if running else 'SpringBoot not running, start via launcher'})
    if name == 'hmi':
        return jsonify({'ok': False, 'message': 'HMI requires manual start: cd 02_上位机/DeviceHMI && dotnet run'})
    return jsonify({'ok': False, 'error': f'Unknown service: {name}'}), 404


if __name__ == '__main__':
    print("IMOGJO调度优化微服务启动")
    print(f"后端地址: {BACKEND_URL}")
    print("接口: POST http://127.0.0.1:5000/api/optimize")
    print("健康检查: GET http://127.0.0.1:5000/api/health")
    app.run(host='127.0.0.1', port=5000, debug=False)
