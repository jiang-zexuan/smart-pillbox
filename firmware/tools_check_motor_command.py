from pathlib import Path
root = Path(__file__).resolve().parent
protocol = (root / 'src' / 'protocol.c').read_text(encoding='utf-8', errors='ignore')
feature = (root / 'include' / 'feature_config.h').read_text(encoding='utf-8', errors='ignore')
main = (root / 'src' / 'main.c').read_text(encoding='utf-8', errors='ignore')
assert '#define FEATURE_MOTOR_RESERVED     1' in feature, 'motor feature should be enabled'
assert 'strcmp(cmd_str, "MOTOR")' in protocol, 'Protocol_ParseLine should dispatch MOTOR commands'
assert 'Protocol_HandleMotor' in protocol, 'protocol.c should implement Protocol_HandleMotor'
assert 'MOTOR_ACK:TEST' in protocol, 'MOTOR:TEST should send an acknowledgement'
assert 'Motor_Step(300' in protocol, 'MOTOR:TEST should step motor without homing'
assert 'Motor_CalibrateHome();' not in main, 'startup should not auto-home while no optical home sensor is installed'
print('motor command static checks passed')
