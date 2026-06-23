"""
Industrial Monitoring System Launcher
Start/Stop/Status management with rich terminal UI
Run: python launcher.py
"""
import os
import sys
import time
import signal
import socket
import subprocess
import webbrowser
from pathlib import Path

try:
    from rich.console import Console
    from rich.table import Table
    from rich.panel import Panel
    from rich.layout import Layout
    from rich.live import Live
    from rich.text import Text
    from rich import box
    RICH = True
except ImportError:
    RICH = False
    print("pip install rich   for better UI")

console = Console() if RICH else None

PROJECT_ROOT = Path(__file__).resolve().parent

# Auto-detect directories, no hardcoded Chinese paths
def find_dir(prefix):
    """Find subdirectory starting with prefix (e.g. '05_')"""
    dirs = sorted([d for d in PROJECT_ROOT.iterdir() if d.is_dir() and d.name.startswith(prefix)])
    return dirs[0] if dirs else None

FLASK_DIR = find_dir('05_')
HMI_DIR = PROJECT_ROOT / '02_上位机' / 'DeviceHMI'
SPRING_DIR = PROJECT_ROOT / 'monitor_server' / 'demo'

SERVICES = {
    'nettoplcsim': {
        'name': 'NetToPLCsim', 'port': 102, 'type': 'manual',
        'cwd': str(PROJECT_ROOT / '01_PLC工程'),
        'exe': 'NetToPLCsim.exe',
        'desc': 'PLC sim bridge (需Admin)',
        'icon': 'PLC',
    },
    'plcsim': {
        'name': 'PLCSIM + TIA', 'port': None, 'type': 'manual',
        'cwd': None,
        'desc': 'TIA Portal下载+切RUN',
        'icon': 'SIM',
    },
    'spring': {
        'name': 'SpringBoot', 'port': 8081, 'type': 'process',
        'cwd': str(SPRING_DIR) if SPRING_DIR.exists() else None,
        'cmd': [str(SPRING_DIR / 'mvnw.cmd'), 'spring-boot:run', '-q'],
        'health': 'http://localhost:8081/api/devices',
        'desc': 'REST API backend',
        'icon': 'API',
    },
    'flask': {
        'name': 'Flask IMOGJO', 'port': 5000, 'type': 'process',
        'cwd': str(FLASK_DIR) if FLASK_DIR else None,
        'cmd': [sys.executable, 'scheduler.py'],
        'health': 'http://127.0.0.1:5000/api/health',
        'desc': 'IMOGJO optimizer',
        'icon': 'ALG',
    },
    'hmi': {
        'name': 'HMI (C#)', 'port': None, 'type': 'manual',
        'cwd': str(HMI_DIR) if HMI_DIR.exists() else None,
        'cmd': ['dotnet', 'run'],
        'process': 'DeviceHMI.exe',
        'desc': 'WinForm SCADA',
        'icon': 'HMI',
    },
}


def env_precheck():
    """Check environment prerequisites, return list of warnings"""
    warnings = []

    # Check Java/mvnw
    spring_cwd = SERVICES['spring'].get('cwd')
    if spring_cwd and Path(spring_cwd).exists():
        mvnw = Path(spring_cwd) / ('mvnw.cmd' if sys.platform == 'win32' else 'mvnw')
        if not mvnw.exists():
            warnings.append(f'Maven wrapper 未找到: {mvnw}，请确认 SpringBoot 项目完整')
    else:
        warnings.append('SpringBoot 项目目录未找到，请确认 monitor_server/demo 路径')

    # Check Python packages
    if not RICH:
        warnings.append('rich 库未安装，终端UI退化 — 运行: pip install rich')

    try:
        import flask
        import flask_cors
    except ImportError:
        warnings.append('flask/flask_cors 未安装 — 运行: pip install flask flask-cors')

    # Check HMI project
    hmi_cwd = SERVICES['hmi'].get('cwd')
    if hmi_cwd and not Path(hmi_cwd).exists():
        warnings.append('HMI 上位机项目未找到，C#仿真服务不可用')

    return warnings


def print_env_warnings(warnings):
    """Display environment warnings with Chinese hints"""
    if not warnings:
        if RICH:
            console.print('[green]Environment check OK[/green]')
        else:
            print('Environment check OK')
        return

    if RICH:
        console.print()
        for w in warnings:
            console.print(f'  [yellow][WARN][/yellow] {w}')
        console.print()
    else:
        print()
        for w in warnings:
            print(f'  [WARN] {w}')
        print()


def check_port(port):
    """Check if a port is listening using netstat"""
    if port is None:
        return False
    try:
        result = subprocess.run(
            ['netstat', '-ano'], capture_output=True, text=True, timeout=5,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
        )
        for line in result.stdout.split('\n'):
            if f':{port}' in line and 'LISTENING' in line:
                return True
    except Exception:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(1)
            result = s.connect_ex(('127.0.0.1', port))
            s.close()
            return result == 0
        except Exception:
            pass
    return False


def check_process(name):
    """Check if a process is running by image name"""
    try:
        r = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {name}'],
                         capture_output=True, text=True, timeout=5,
                         creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
        return name.lower() in r.stdout.lower()
    except Exception:
        return False


def get_pid_by_port(port):
    """Get PID of process listening on a port"""
    if port is None:
        return None
    try:
        result = subprocess.run(
            ['netstat', '-ano'], capture_output=True, text=True, timeout=5,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
        )
        for line in result.stdout.split('\n'):
            if f':{port}' in line and 'LISTENING' in line:
                parts = line.strip().split()
                return parts[-1] if parts else None
    except Exception:
        pass
    return None


def kill_by_port(port):
    """Kill process listening on a port"""
    pid = get_pid_by_port(port)
    if pid:
        try:
            subprocess.run(['taskkill', '/PID', pid, '/F'],
                         capture_output=True, timeout=10,
                         creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
            return True
        except Exception:
            pass
    return False


def check_health(url, timeout=3):
    """Check HTTP health endpoint"""
    import urllib.request
    try:
        req = urllib.request.Request(url, method='GET')
        urllib.request.urlopen(req, timeout=timeout)
        return True
    except Exception:
        return False


def service_status(svc_key):
    """Get status of a service: running/stopped/unknown"""
    svc = SERVICES[svc_key]
    name = svc['name']

    if svc['type'] == 'system':
        try:
            r = subprocess.run(['sc', 'query', svc['name']], capture_output=True, text=True,
                             creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
            if 'RUNNING' in r.stdout:
                return 'running'
            return 'stopped'
        except Exception:
            return 'unknown'

    if svc.get('port'):
        running = check_port(svc['port'])
        if running:
            if svc.get('health'):
                if check_health(svc['health']):
                    return 'running'
                return 'starting'
            return 'running'
        return 'stopped'

    # Check by process name for manual services
    if svc.get('process'):
        if check_process(svc['process']):
            return 'running'
        return 'stopped'

    return 'stopped'


def status_label(status):
    """Return rich-formatted status label"""
    if RICH:
        if status == 'running':
            return Text('RUNNING', style='bold green')
        elif status == 'stopped':
            return Text('STOPPED', style='dim white')
        elif status == 'starting':
            return Text('STARTING', style='bold yellow')
        return Text('UNKNOWN', style='red')
    else:
        c = {'running': 'OK', 'stopped': '--', 'starting': '..', 'unknown': '??'}
        return c.get(status, '??')


def print_banner():
    if RICH:
        console.print()
        console.print(Panel(
            "[bold cyan]Full-Stack Industrial Monitoring System[/bold cyan]\n"
            "[dim]IMOGJO Multi-Objective Optimization  |  OEE  |  SCADA[/dim]",
            box=box.DOUBLE,
            border_style="cyan",
            padding=(1, 2),
        ))
    else:
        print("\n=== Industrial Monitoring System Launcher ===\n")


def print_status_table():
    """Print status of all services as a table"""
    if RICH:
        table = Table(title="Service Status", box=box.ROUNDED, border_style="cyan",
                      title_style="bold cyan")
        table.add_column("Service", style="bold", width=18)
        table.add_column("Port", width=8)
        table.add_column("Status", width=12)
        table.add_column("PID", width=8)
        table.add_column("Role", style="dim", width=22)

        for key, svc in SERVICES.items():
            status = service_status(key)
            pid = None
            if svc.get('port'):
                pid = get_pid_by_port(svc['port'])
            elif svc.get('process'):
                try:
                    r = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {svc["process"]}'],
                                     capture_output=True, text=True,
                                     creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
                    for line in r.stdout.split('\n'):
                        if svc['process'].lower() in line.lower():
                            parts = line.strip().split()
                            if parts and parts[-1].isdigit():
                                pid = parts[-1]
                            break
                except Exception:
                    pass
            elif svc['type'] == 'system':
                pid = '(svc)'
            else:
                pid = '--'
            table.add_row(
                f"[bold]{svc['icon']}[/bold] {svc['name']}",
                str(svc['port']) if svc.get('port') else '--',
                status_label(status),
                str(pid) if pid else '--',
                svc['desc'],
            )

        console.print(table)
    else:
        print(f"{'Service':<18} {'Port':<8} {'Status':<10} {'PID':<8}")
        for key, svc in SERVICES.items():
            st = service_status(key)
            pid = '--'
            print(f"{svc['name']:<18} {str(svc.get('port','--')):<8} {st:<10} {str(pid):<8}")


def print_startup_order():
    """Show manual steps that must be done before auto-start"""
    if RICH:
        console.print()
        console.print("[bold yellow]启动前手动操作:[/bold yellow]")
        console.print("  [1] NetToPLCsim  [bold red]管理员运行[/bold red] → Add站点 → Start Server")
        console.print("  [2] TIA Portal → PLCSIM → 下载程序 → CPU切 [bold green]RUN[/bold green]")
        console.print("  [3] 然后运行 [bold cyan]python launcher.py start[/bold cyan] 一键启动后端")
        console.print()


def start_service(svc_key):
    """Start a single service, return (success, message)"""
    svc = SERVICES[svc_key]
    name = svc['name']

    if svc['type'] == 'system':
        # System service: just check
        if service_status(svc_key) == 'running':
            return True, f"{name} already running"
        try:
            subprocess.run(['net', 'start', name], capture_output=True, timeout=30,
                         creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
            return True, f"{name} started"
        except Exception as e:
            return False, f"Failed to start {name}: {e}"

    if svc['type'] == 'manual':
        return True, f"{name} requires manual start:\n       cd {svc.get('cwd','')} && {' '.join(svc['cmd'])}"

    # Process type
    if service_status(svc_key) == 'running':
        return True, f"{name} already running on port {svc['port']}"

    if svc.get('cwd') is None or not Path(svc['cwd']).exists():
        return False, f"{name} directory not found: {svc.get('cwd', '?')}"

    try:
        cwd = svc['cwd']
        subprocess.Popen(
            svc['cmd'],
            cwd=cwd,
            creationflags=subprocess.CREATE_NO_WINDOW | subprocess.DETACHED_PROCESS if sys.platform == 'win32' else 0,
        )
        return True, f"{name} launched (background)"
    except Exception as e:
        return False, f"Failed to start {name}: {e}"


def stop_service(svc_key):
    """Stop a single service, return (success, message)"""
    svc = SERVICES[svc_key]
    name = svc['name']

    if svc['type'] == 'system':
        return True, f"{name} kept running (system service)"

    if svc.get('port'):
        if kill_by_port(svc['port']):
            time.sleep(0.5)
            if not check_port(svc['port']):
                return True, f"{name} stopped"
            return False, f"{name} may still be running"
        return True, f"{name} was not running"

    return True, f"{name}: no action needed"


def wait_for_service(svc_key, timeout_sec=120):
    """Wait for service to become healthy"""
    svc = SERVICES[svc_key]
    if svc.get('health') is None:
        return check_port(svc['port']) if svc.get('port') else False

    elapsed = 0
    while elapsed < timeout_sec:
        if check_health(svc['health']):
            return True
        time.sleep(1)
        elapsed += 1
    return False


def start_all():
    """Start all services in order"""
    console.clear() if RICH else None
    print_banner()
    print_startup_order()

    warnings = env_precheck()
    if warnings:
        print_env_warnings(warnings)
        if RICH:
            console.print('[dim]Continue anyway...[/dim]\n')

    if RICH:
        console.print("[bold]Starting auto services...[/bold]\n")

    results = []
    for key in SERVICES:
        svc = SERVICES[key]
        name = svc['name']

        # Skip manual services
        if svc['type'] == 'manual':
            continue

        if RICH:
            console.print(f"  [{svc['icon']}] {name}...", end=" ")

        # Skip if already running
        if service_status(key) == 'running':
            if RICH:
                console.print("[green]already running[/green]")
            results.append((key, True, 'skipped'))
            continue

        success, msg = start_service(key)

        if success and svc['type'] == 'process' and svc.get('health'):
            if RICH:
                with console.status(f"[cyan]Waiting for {name}...[/cyan]"):
                    ready = wait_for_service(key)
            else:
                ready = wait_for_service(key)
            if ready:
                if RICH:
                    console.print(f"  [{svc['icon']}] {name} [green]READY[/green]")
            else:
                if RICH:
                    console.print(f"  [{svc['icon']}] {name} [yellow]TIMEOUT[/yellow]")
            results.append((key, ready, 'process'))
        else:
            if RICH and success:
                console.print(f"  [green]OK[/green] {msg}")
            elif RICH:
                console.print(f"  [red]FAILED[/red] {msg}")
            results.append((key, success, svc['type']))

    # Open dashboard
    time.sleep(1)
    if service_status('spring') == 'running':
        console.print("\n[cyan]Opening dashboard...[/cyan]")
        webbrowser.open('http://localhost:8081/dashboard.html')

    if RICH:
        console.print("\n[bold green]Startup complete.[/bold green]")
        print_status_table()
    return results


def stop_all():
    """Stop all services"""
    console.clear() if RICH else None
    print_banner()

    if RICH:
        console.print("[bold]Stopping all services...[/bold]\n")

    for key in SERVICES:
        svc = SERVICES[key]
        if svc['type'] == 'manual':
            continue
        if RICH:
            console.print(f"  [{svc['icon']}] {svc['name']}...", end=" ")
        success, msg = stop_service(key)
        if RICH:
            if success:
                console.print(f"[green]{msg}[/green]")
            else:
                console.print(f"[red]{msg}[/red]")

    if RICH:
        console.print("\n[bold green]All services stopped.[/bold green]")


def interactive_menu():
    """Interactive menu loop"""
    while True:
        console.clear() if RICH else None
        print_banner()
        print_startup_order()
        print_status_table()

        if RICH:
            console.print()
            console.print("  [bold][1][/bold] Start All   [bold][2][/bold] Stop All   [bold][3][/bold] Restart All")
            console.print("  [bold][4][/bold] Status      [bold][5][/bold] Dashboard  [bold][6][/bold] Exit")
            console.print()

        choice = input("  Choice [1-6]: ").strip() if not RICH else input("  Choice [1-6]: ").strip()

        if choice == '1':
            start_all()
            input("\n  Press Enter to continue...")
        elif choice == '2':
            stop_all()
            input("\n  Press Enter to continue...")
        elif choice == '3':
            stop_all()
            time.sleep(2)
            start_all()
            input("\n  Press Enter to continue...")
        elif choice == '4':
            console.clear() if RICH else None
            print_banner()
            print_status_table()
            input("\n  Press Enter to continue...")
        elif choice == '5':
            webbrowser.open('http://localhost:8081/dashboard.html')
            console.print("[green]Dashboard opened in browser[/green]")
            time.sleep(1)
        elif choice == '6':
            console.print("[cyan]Goodbye.[/cyan]")
            break
        else:
            console.print("[red]Invalid choice[/red]")
            time.sleep(0.5)


if __name__ == '__main__':
    import argparse
    p = argparse.ArgumentParser(description='Industrial Monitoring System Launcher')
    p.add_argument('action', nargs='?', default='menu',
                   choices=['menu', 'start', 'stop', 'restart', 'status'],
                   help='Action to perform (default: menu)')
    args = p.parse_args()

    if args.action == 'start':
        start_all()
    elif args.action == 'stop':
        stop_all()
    elif args.action == 'restart':
        stop_all()
        time.sleep(2)
        start_all()
    elif args.action == 'status':
        print_banner()
        print_startup_order()
        print_status_table()
    else:
        interactive_menu()
