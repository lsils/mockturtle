#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Solves a binpacking problem using the CP-SAT solver."""

import re
import os
import sys
from collections import defaultdict
from ortools.sat.python import cp_model
from pprint import pprint

class dff:
    __slots__ = ['fanin', 'fanout', 'sigma']
    def __init__(self, fanin : int, fanout : int, sigma : int) -> None:
        self.fanin = int(fanin)
        self.fanout = int(fanout)
        self.sigma = int(sigma)
        
    def __repr__(self) -> str :
        return f"var_{self.fanin}_{self.fanout}_{self.sigma}"
    
def from_str(varname : str) -> dff:
    _, fanin, fanout, sigma = varname.split('_')
    return dff( fanin, fanout, sigma )

def parse(file_path : str, nphases : int):
    all_vars = set()
    phase_constr = []
    # required = []
    buffer_constr = []
    t1_constr = defaultdict(list)
    
    gate_vars = set()
    sa_dffs = set()
    t1_required_dff_constr = []
    
    
    with open(file_path, 'r') as file:
        for line in file:
            line = line.strip()
            if not line:
                continue
            
            constraint_type, *items = line.split(',')
            
            if constraint_type == 'GATE':
                varname = items[0]
                var = from_str(varname)
                all_vars.add(var)
                gate_vars.add(var)
                
            if constraint_type == 'SADFF':
                varname = items[0]
                var = from_str(varname)
                all_vars.add(var)
                sa_dffs.add(var)
                
            if constraint_type == 'PHASE':
                vars_here = []
                for varname in items:
                    var = from_str(varname)
                    all_vars.add(var)
                    vars_here.append(var)
                phase_constr.append(vars_here)
                # verify that all of them have the same phase
                sigmas = {var.sigma for var in vars_here}
                assert(len(sigmas) == 1)
                
            if constraint_type == 'BUFFER':
                vars_here = []
                for varname in items:
                    var = from_str(varname)
                    all_vars.add(var)
                    vars_here.append(var)
                buffer_constr.append(vars_here)
                
                sigmas = {var.sigma for var in vars_here}
                # verify that there are <nphases> different sigmas
                assert(len(sigmas) == nphases)
                # verify that the difference is <nphases - 1>
                assert(max(sigmas) - min(sigmas) == nphases - 1)
            
            if constraint_type == 'AT_LEAST_ONE':
                vars_here = []
                for varname in items:
                    var = from_str(varname)
                    all_vars.add(var)
                    vars_here.append(var)
                t1_required_dff_constr.append(vars_here)
                
                sigmas = {var.sigma for var in vars_here}
                # verify that there are fewer than <nphases> different sigmas
                assert(len(sigmas) <= nphases)
                # verify that the difference is <= <nphases - 1>
                assert(max(sigmas) - min(sigmas) <= nphases - 1)
                
            
            if constraint_type == 'T1_FANIN':
                gate_name, *varnames = items
                vars_here = []
                for varname in varnames:
                    var = from_str(varname)
                    all_vars.add(var)
                    vars_here.append(var)
                t1_constr[gate_name].append(vars_here)
                
                sigmas = {var.sigma for var in vars_here}
                # verify that the difference is less than <nphases>
                assert(max(sigmas) - min(sigmas) < nphases )
    
    return all_vars, gate_vars, sa_dffs, phase_constr, buffer_constr, t1_required_dff_constr, t1_constr

# Example usage
if __name__ == "__main__":
    
    # filename = sys.argv[1]
    # NPH = sys.argv[2]
    
    filename = "/Users/brainkz/Documents/GitHub/mockturtle_alessandro/build/adder_paths_4.csv"
    NPH = 7
    
    # Split the filename and its extension
    base_name, _ = os.path.splitext(filename)
    # Create the new filename with the ".log" extension
    log_filename = base_name + ".log"
    # Save the original sys.stdout
    original_stdout = sys.stdout
    # Open the log file in write mode
    log_file = open(log_filename, 'w')
    # Redirect sys.stdout to the log file
    # sys.stdout = log_file
    
    model = cp_model.CpModel()
    
    all_vars, gate_vars, sa_dffs, phase_constr, buffer_constr, t1_required_dff_constr, t1_constr = parse(filename, NPH)
    
    cpsat_vars = {var : model.NewBoolVar(str(var)) for var in all_vars}
    
    # process the constraints
    
    minimized_vars = {cpsat_vars[var] for var in all_vars if var not in gate_vars}
    
    for var in all_vars:
        if var in gate_vars:
            model.Add( cpsat_vars[var] == True )
            
        if var in sa_dffs:
            model.Add( cpsat_vars[var] == True )
    
    for vars in phase_constr:
        model.AddAtMostOne([cpsat_vars[var] for var in vars])
        print(f"AtMostOne: {[var for var in vars]}")
        
    for vars in buffer_constr:
        model.AddBoolOr([cpsat_vars[var] for var in vars])
        print(f"OR: {[var for var in vars]}")
        
    for vars in t1_required_dff_constr:
        model.AddBoolOr([cpsat_vars[var] for var in vars])
        print(f"OR: {[var for var in vars]}")
        
    
    for t1_gate, branches in t1_constr.items():
        Lvars = []
        
        for branch_ID, branch in enumerate(branches):
            
            solver = cp_model.CpSolver()
            status = solver.Solve(model)
            print(f'[Checkpoint {t1_gate} {branch_ID}] Solve status: {solver.StatusName(status)}')
            # if status == cp_model.OPTIMAL:
            print(f'[Checkpoint {t1_gate} {branch_ID}] Objective value: %i' % solver.ObjectiveValue())
            
            # will contain the variables split into sigmas
            buckets = defaultdict(list)
            for var in branch:
                buckets[var.sigma].append(var)
            
            sorted_buckets = sorted(buckets.items(), reverse=True)
            
            pprint(sorted_buckets)
            
            # First, create the sequence of combined variables a0, a1, ... etc.
            var_sequence = []  
            negated_var_sequence = [] 
            for i, (sigma, vars) in enumerate(sorted_buckets):
                if len(vars) == 1:
                    var = cpsat_vars[vars[0]]
                    var_sequence.append( var )
                    
                    negated_var = model.NewBoolVar(f"{var.Name()}_NOT")
                    model.Add(negated_var == True).OnlyEnforceIf( var.Not() )
                    negated_var_sequence.append( negated_var )
                else:
                    combined_var = model.NewBoolVar(f"COMB_{t1_gate}_{branch_ID}_{sigma}")
                    model.AddBoolOr( [cpsat_vars[var] for var in vars] )
                    var_sequence.append( combined_var )
            
                    negated_var = model.NewBoolVar(f"{combined_var.Name()}_NOT")
                    model.Add(negated_var == True).OnlyEnforceIf( combined_var.Not() )
                    negated_var_sequence.append( negated_var )
            
            
            print('var_sequence')
            pprint(var_sequence, indent=4)
            print('negated_var_sequence')
            pprint(negated_var_sequence, indent=4)
            
            
            
            # Next, create the sequence of multiplicative terms a0, (!a0)a1, (!a0)(!a1)a2, ...
            mult_terms = []
            conditions = []
            for i, cpsat_var in enumerate(var_sequence):
                term = model.NewIntVar(0, 1000, f"MULT_{t1_gate}_{branch_ID}_{i}")
                
                condition = model.NewBoolVar(f"IS_LAST_{t1_gate}_{branch_ID}_{i}")
                model.AddBoolAnd([cpsat_var, *negated_var_sequence[:i]]).OnlyEnforceIf( term == i+1 )
                conditions.append(condition)
            
                # negated_prior_vars = negated_var_sequence[:i]
                # model.AddMultiplicationEquality(term, [i+1, cpsat_var, *negated_prior_vars])
                mult_terms.append( term )

                print('mult_terms')
                pprint(mult_terms, indent=4)
                
            Lvar = model.NewIntVar( 0, 1000, f"LVAR_{t1_gate}_{branch_ID}" )
            model.Add( Lvar==sum(mult_terms) )
            model.Add( Lvar > 0 )
            
            Lvars.append(Lvar)
            
        model.AddAllDifferent(Lvars)
                
    model.Minimize(sum(minimized_vars))

    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    print(f'Solve status: {solver.StatusName(status)}')
    # if status == cp_model.OPTIMAL:
    print('Objective value: %i' % solver.ObjectiveValue())
    
    # Restore sys.stdout to the original value
    sys.stdout = original_stdout
    # Close the log file
    log_file.close()
    
    print(f'Solve status: {solver.StatusName(status)}')
    # if status == cp_model.OPTIMAL:
    print('Objective value: %i' % solver.ObjectiveValue())
    # print('Statistics')
    # print(f'  - conflicts : {solver.NumConflicts():d}')
    # print(f'  - branches  : {solver.NumBranches():d}')
    # print(f'  - wall time : {solver.WallTime():f} s')

    # var_dict = {var.Name(): var for var in all_vars}

    # for varname in sorted(var_dict):
    #     print(varname, '=', solver.BooleanValue(var_dict[varname]))
        

    # for vars in buffer_constr:
    #     var_dict_2 = {var.Name(): var for var in vars}
    #     print("AddAtLeastOne :")
    #     for varname in sorted(var_dict_2):
    #         print(varname, '=', solver.BooleanValue(var_dict_2[varname]))
    # sys.exit(int(solver.ObjectiveValue()))