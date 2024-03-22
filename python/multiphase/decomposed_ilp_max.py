
from ortools.sat.python import cp_model
from collections import defaultdict
import sys
import os
from pprint import pprint
# import builtins

# LOG_FILE = "ilp_log.log"

# def print(*args, **kwargs):
#     with open("ilp_log.log", "w") as f:
#         f.write()

EPS = 1e-6

TYPES = {"0": "PI", "1": "AA", "2": "AS", "3": "SA"} #"1": "PO",
# TYPES = {"0": "AA", "1": "AS", "2": "SA"}

class Primitive:
    __slots__ = ['sig', 'type', 'fanins', 'fanouts']
    def __init__(self, _sig: int, _type : str, _fanins : list, _fanouts : list) -> None:
        self.sig        = _sig
        self.type       = _type
        self.fanins     = _fanins
        self.fanouts    = _fanouts

    def __eq__(self, other) -> bool: 
        if self.sig == other.sig:
            assert(self.type == other.type)
            assert(self.fanins == other.fanins)
            assert(self.fanouts == other.fanouts)
            return True
        return False

    def __neq__(self, other) -> bool: 
        if self.sig == other.sig:
            assert(self.type == other.type)
            assert(self.fanins == other.fanins)
            assert(self.fanouts == other.fanouts)
            return False
        return True

    def __repr__(self):
        return f"Primitive(sig={self.sig}, type='{self.type}', fanins={self.fanins}), fanouts={self.fanouts}"
    
def parse_specs(filename: str) -> dict[int, Primitive]:
    items = {}
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            
            list_split = line.split(',')
            _sig_str, _type_str, _fanin_str, _fanout_str = list_split
            _sig  = int(_sig_str)
            _fanins = [int(_fanin_sig) for _fanin_sig in _fanin_str.split('|') if _fanin_sig]
            _fanouts = [int(_fanout_sig) for _fanout_sig in _fanout_str.split('|') if _fanout_sig]
            
            g = Primitive(_sig, TYPES[_type_str], _fanins, _fanouts)
            items[g.sig] = g
    return items

if __name__ == "__main__":
    
    # Model.
    model = cp_model.CpModel()
    
    NPH = int(sys.argv[1])
    cfg_path = sys.argv[2]
    
    # NPH = 4
    # S_bounds = ( 0, 1000//NPH )      #Clock stage bounds
    # phi_bounds = 0, NPH - 1    #Phase bounds
    
    # Split the filename and its extension
    base_name, _ = os.path.splitext(cfg_path)
    # Create the new filename with the ".log" extension
    log_filename = base_name + ".log"
    # Save the original sys.stdout
    original_stdout = sys.stdout
    # Open the log file in write mode
    log_file = open(log_filename, 'w')
    # Redirect sys.stdout to the log file
    sys.stdout = log_file

    print(f"Parsing specs file {cfg_path}")
    all_signals = parse_specs(cfg_path)
    # print(all_signals)
    
    print(f'Finished creating [all_signals]')
    pprint(all_signals)
    
    sigma_bounds = (0, 1000)

    print(f"Setting up the model {cfg_path}")
    fanout = defaultdict(int)
    Sigma = {}
    fanin_constr = {}
    ctr = 0
    for g in all_signals.values():
        if g.type == 'PI':
            print(f"Creating PI SIGMA_{g.sig}")
            Sigma[g.sig] = model.NewIntVar(0, NPH - 1, f"SIGMA_{g.sig}")
        else:
            print(f"Creating {g.type} gate SIGMA_{g.sig}")
            Sigma[g.sig] = model.NewIntVar(*sigma_bounds, f"SIGMA_{g.sig}")
            for p_sig in g.fanins:
                fanout[p_sig] += 1
    
    print(f'Finished creating [Sigma]')
    pprint(Sigma)
    
    fanin_diff = {}
    Delta = {}
    # IMPORTANT: PIs are assumed to be at clock stage 0 but with an arbitrary phase
    expr = []
    for g in all_signals.values():
        # print(f"The type of the gate {g.sig} is {g.type}")
        if g.type == 'AA':
            # any PI is the earliest fanin, with zero phase and zero clock stage
            # IMPORTANT: only 2-input CB is supported
            assert(len(g.fanins) == 2)
            a_sig, b_sig = g.fanins
            a = all_signals[a_sig]
            b = all_signals[b_sig]
            
            max_diff = model.NewIntVar(*sigma_bounds, f'max_{a.sig}_{b.sig}')
            model.AddMaxEquality(max_diff, [ Sigma[g.sig] - Sigma[a.sig], 
                                            Sigma[g.sig] - Sigma[b.sig] ])
            # expr.append( max_diff )
            
            div_val = model.NewIntVar(*sigma_bounds, f'div_{a.sig}_{b.sig}')
            model.AddDivisionEquality(div_val, max_diff, NPH)
            expr.append( div_val )
            
            # print(f"AA constraint: {Sigma[a.sig].Name()} <= {Sigma[g.sig].Name()}")
            # print(f"AA constraint: {Sigma[b.sig].Name()} <= {Sigma[g.sig].Name()}")
            
            model.Add( Sigma[a.sig] <= Sigma[g.sig] )
            model.Add( Sigma[b.sig] <= Sigma[g.sig] )
            
        # Regular gate - use the standard definition
        elif g.type == 'AS':
            for i, p_sig in enumerate(g.fanins):
                # print(f"AS constraint: {Sigma[p_sig].Name()} < {Sigma[g.sig].Name()}")
                model.Add(Sigma[p_sig] < Sigma[g.sig])
                
                # expr.append( Sigma[g.sig] - Sigma[p_sig] )
                
                delta = model.NewIntVar(*sigma_bounds, f'delta_{p_sig}_{g.sig}')
                model.Add(delta == (Sigma[g.sig] - Sigma[p_sig]))
                
                div_val = model.NewIntVar(*sigma_bounds, f'div_{p_sig}_{g.sig}')
                model.AddDivisionEquality(div_val, delta, NPH)
                expr.append( div_val )
                
        elif g.type == 'SA':
            
            for i, p_sig in enumerate(g.fanins):
                p = all_signals[p_sig]
                if (p.type == 'AS') and (fanout[p.sig] == 1):
                    model.Add(Sigma[p_sig] <= Sigma[g.sig])
                    # print(f"SA constraint: {Sigma[p_sig].Name()} <= {Sigma[g.sig].Name()} ({p.type}) ({fanout[p.sig]})")
                else: 
                    model.Add(Sigma[p_sig] <  Sigma[g.sig])
                    # print(f"SA constraint: {Sigma[p_sig].Name()} <  {Sigma[g.sig].Name()} ({p.type}) ({fanout[p.sig]})")
                    
                # expr.append( Sigma[g.sig] - Sigma[p_sig] )
                    
                delta = model.NewIntVar(*sigma_bounds, f'delta_sa_{p_sig}_{g.sig}')
                model.Add(delta == (Sigma[g.sig] - Sigma[p_sig] + NPH - 1))
                
                div_val = model.NewIntVar(*sigma_bounds, f'div_{p_sig}_{g.sig}')
                model.AddDivisionEquality(div_val, delta, NPH)
                expr.append( div_val )
            

    model.Minimize(sum(expr))

    
    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    print(f'Starting Macro ILP')
    solver.parameters.max_time_in_seconds = 300.0
    status = solver.Solve(model)
    print(f'Solve status: {solver.StatusName(status)}')
    if (solver.StatusName(status) in ("OPTIMAL", "FEASIBLE")):
        print(f'Objective value: {solver.ObjectiveValue()}')
        for sig, sigma in Sigma.items():
            g = all_signals[sig]
            print(f'{sigma.Name()[6:]} :{solver.Value(sigma)}')
            
    # Restore sys.stdout to the original value
    sys.stdout = original_stdout
    # Close the log file
    log_file.close()
    
    print(f'Solve status: {solver.StatusName(status)}')
    if (solver.StatusName(status) in ("OPTIMAL", "FEASIBLE")):
        print(f'Objective value: {solver.ObjectiveValue()}')
        for sig, sigma in Sigma.items():
            g = all_signals[sig]
            print(f'{sigma.Name()[6:]} :{solver.Value(sigma)}')
