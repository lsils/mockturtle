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

    def __hash__(self):
        return hash((self.fanin, self.fanout, self.sigma))

    def __repr__(self) -> str :
        return f"var_{self.fanin}_{self.fanout}_{self.sigma}"
    
    def __eq__(self, other):
        return self.fanin == other.fanin and self.fanout == other.fanout and self.sigma == other.sigma
    
def from_str(varname : str) -> dff:
    _, fanin, fanout, sigma = varname.split('_')
    return dff( fanin, fanout, sigma )

def parse(file_path : str, nphases : int):
    all_vars = set()
    phase_constr = []
    # required = []
    buffer_constr = []
    t1_constr = defaultdict(list)
    
    helper_vars = set()
    sa_dffs = set()
    t1_required_dff_constr = []
    
    
    with open(file_path, 'r') as file:
        for line in file:
            line = line.strip()
            if not line:
                continue
            
            constraint_type, *items = line.split(',')
            
            if constraint_type == 'HELPER':
                varname = items[0]
                var = from_str(varname)
                all_vars.add(var)
                helper_vars.add(var)
                
            if constraint_type == 'SA_REQUIRED':
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
            
            if constraint_type in ('INVERTED_INPUT', 'AT_LEAST_ONE'):
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
    
    return all_vars, helper_vars, sa_dffs, phase_constr, buffer_constr, t1_required_dff_constr, t1_constr

# Example usage
if __name__ == "__main__":
    
    filename = sys.argv[1]
    NPH = int(sys.argv[2])
    
    # filename = "/Users/brainkz/Documents/GitHub/mockturtle_alessandro/build/adder_paths_4.csv"
    # NPH = 7
    
    # Split the filename and its extension
    base_name, _ = os.path.splitext(filename)
    # Create the new filename with the ".log" extension
    log_filename = base_name + ".log"
    # Save the original sys.stdout
    original_stdout = sys.stdout
    # Open the log file in write mode
    log_file = open(log_filename, 'w')
    # Redirect sys.stdout to the log file
    sys.stdout = log_file
    
    model = cp_model.CpModel()
    
    all_vars, helper_vars, sa_dffs, phase_constr, buffer_constr, t1_required_dff_constr, t1_constr = parse(filename, NPH)
    
    cpsat_vars = {var : model.NewBoolVar(str(var)) for var in all_vars}
    
    print('all_vars')
    pprint(all_vars, indent=4)
    
    print('helper_vars')
    pprint(helper_vars, indent=4)
    
    print('sa_dffs')
    pprint(sa_dffs, indent=4)
    
    # process the constraints
    
    minimized_vars = {cpsat_vars[var] for var in all_vars if var not in helper_vars}
    
    for var in all_vars:
        if var in helper_vars:
            model.AddBoolAnd( [cpsat_vars[var]] )
            
        if var in sa_dffs:
            model.AddBoolAnd( [cpsat_vars[var]] )
    
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
        last_sigmas = []
        for branch_ID, branch in enumerate(branches):
            
            # will contain the variables split into sigmas
            buckets = defaultdict(list)
            for var in branch:
                buckets[var.sigma].append(var)
            
            sorted_buckets = sorted(buckets.items(), reverse=True)
            
            pprint(sorted_buckets)
            
            branch_sigmas = []
            for i, (sigma, vars) in enumerate(sorted_buckets):
                for j, var in enumerate(vars):
                    sigma_var = model.NewIntVar(-1, sigma, f"{t1_gate}_{branch_ID}_{sigma}_{j}")
                    model.Add( sigma_var == sigma ).OnlyEnforceIf(cpsat_vars[var])
                    model.Add( sigma_var == -1    ).OnlyEnforceIf(cpsat_vars[var].Not())
                    branch_sigmas.append(sigma_var)
            
            max_sigma = max(sigma for sigma, _ in sorted_buckets)
            
            last_sigma = model.NewIntVar(-1, max_sigma, f"{t1_gate}_{branch_ID}_{sigma}_{j}")
            model.AddMaxEquality(last_sigma, branch_sigmas)        
            last_sigmas.append(last_sigma)
        
        model.AddAllDifferent(last_sigmas)
                
    model.Minimize(sum(minimized_vars))

    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = float(sys.argv[3])
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
    
    # if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
    #     for var, cpsat_var in cpsat_vars.items():
    #         print(f"{var} = {solver.Value(cpsat_var)}")
    #     for cpsat_var in last_sigmas:
    #         print(f"{cpsat_var.Name()} = {solver.Value(cpsat_var)}")
            
            
    
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