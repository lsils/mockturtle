#!/usr/bin/env python3
# 
"""Solves a multiphase DFF placement problem using CP-SAT."""

import re
import sys
from ortools.sat.python import cp_model

def parse_file_and_create_variables(file_path : str, model : cp_model.CpModel):
    all_varnames = set()
    phase_constr = []
    required = []
    buffer_constr = []
    with open(file_path, 'r') as file:
        for line in file:
            line = line.strip()
            if not line:
                continue

            constraint_type, *variables = line.split(',')

            if constraint_type == 'PHASE':
                # vars = [model.NewBoolVar(var_name) for var_name in variables]
                vars = [var_name for var_name in variables]
                all_varnames.update(vars)
                phase_constr.append(vars)
                # model.AddAtMostOne(vars)
            
            elif constraint_type == 'SA_REQUIRED':
                var_name = variables[-1]
                # var = model.NewBoolVar(var_name)
                all_varnames.add(var_name)
                required.append(var_name)
                    
            elif constraint_type == 'BUFFER':
                buffer_vars = []
                for i, var_expr in enumerate(variables):
                    var_names = parse_expression_and_create_variables(i, model, var_expr)
                    all_varnames.update(var_names)
                    buffer_vars.append(var_names)
                    # comb, vars = parse_expression_and_create_variables(i, model, var_expr)
                    # all_varnames.update(vars)
                    # if i == 0:
                    #     buffer_vars.append(comb)
                    # else:
                    #     buffer_vars.append(comb)
                buffer_constr.append(buffer_vars)
                # model.AddAtLeastOne(buffer_vars)
                
                
    return all_varnames, phase_constr, buffer_constr, required

def parse_expression_and_create_variables(i : int, model : cp_model.CpModel, expression : str):
    # Regular expression to find variable names
    var_pattern = r'var_\d+_\d+_\d+'
    var_names = re.findall(var_pattern, expression)
    
    return var_names
    
    vars = [model.NewBoolVar(var_name) for var_name in var_names]

    if len(vars) > 1:
        long_varname = "__".join(var_names)
        combination = model.NewBoolVar(long_varname)
        model.Add(sum(vars) == 1).OnlyEnforceIf(combination)
        model.Add(sum(vars) == 0).OnlyEnforceIf(combination.Not())
        return combination, vars
    
    else:
        return vars[0], vars

# Example usage
if __name__ == "__main__":
    
    filename = sys.argv[1]
    
    model = cp_model.CpModel()
    all_varnames, phase_constr, buffer_constr, required = parse_file_and_create_variables(filename, model)
    
    all_vars = {var_name : model.NewBoolVar(var_name) for var_name in all_varnames}
    
    for var_names in phase_constr:
        vars = [all_vars[var_name] for var_name in var_names]
        model.AddAtMostOne(vars)
        
    for var_name in required:
        model.Add(all_vars[var_name] == 1)

    for clauses in buffer_constr:
        # var_dict = {var.Name(): var for var in vars}
        # print("AddAtLeastOne :", " ".join([varname for varname in sorted(var_dict)]))
        vars = []
        for var_names in clauses:
            if len(var_names) == 1:
                vars.append(all_vars[var_names[0]])
            else:
                local_vars = [all_vars[var_name] for var_name in var_names]
                long_varname = '__'.join(var_name for var_name in var_names)
                long_var = model.NewBoolVar(long_varname)
                model.AddBoolOr(local_vars).OnlyEnforceIf(long_var)
                model.Add(sum(local_vars) == 0).OnlyEnforceIf(long_var.Not())
                vars.append(long_var)
                
        model.AddBoolOr(vars)
        

    model.Minimize(sum(all_vars.values()))

    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = float(sys.argv[2])
    status = solver.Solve(model)
    print(f'Solve status: {solver.StatusName(status)}')
    # if status == cp_model.OPTIMAL:
    print('Objective value: %i' % solver.ObjectiveValue())
    # print('Statistics')
    # print(f'  - conflicts : {solver.NumConflicts():d}')
    # print(f'  - branches  : {solver.NumBranches():d}')
    # print(f'  - wall time : {solver.WallTime():f} s')

    # var_dict = {varobj.Name(): var for var, varobj in all_vars.items()}

    # print(all_vars)
    for varname, varobj in sorted(all_vars.items()):
        value = solver.Value(varobj)
        if value == 1:
            print(varname)
        # print(varname, '=', solver.Value(varobj))
        

    # for vars in buffer_constr:
    #     var_dict_2 = {var.Name(): var for var in vars}
    #     print("AddAtLeastOne :")
    #     for varname in sorted(var_dict_2):
    #         print(varname, '=', solver.BooleanValue(var_dict_2[varname]))
    # sys.exit(int(solver.ObjectiveValue()))