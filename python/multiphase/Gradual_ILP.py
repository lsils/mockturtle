
from ortools.sat.python import cp_model
from collections import defaultdict

EPS = 1e-6

TYPES = {"0": "AA", "1": "AS", "2": "SA"}

class Primitive:
    __slots__ = ['sig', 'func', 'type', 'fanins']
    def __init__(self, _sig: int, _func : int, _type : str, _fanins : list) -> None:
        self.sig = _sig
        self.func = _func
        self.type = _type
        self.fanins = _fanins
    def __eq__(self, other) -> bool: 
        if self.sig == other.sig:
            assert(self.func == other.func)
            assert(self.type == other.type)
            assert(self.fanins == other.fanins)
            return True
        return False
    def __neq__(self, other) -> bool: 
        if self.sig == other.sig:
            assert(self.func == other.func)
            assert(self.type == other.type)
            assert(self.fanins == other.fanins)
            return False
        return True

def parse_specs(filename: str) -> list[Primitive]:
    items = {}
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('PI'):
                _, _sig_str = line.split()
                _sig = int(_sig_str)
                g = Primitive(_sig, 0xAAAA, 'PI', [])
            else:
                _sig_str, _func_str, _type_str, _fanin_str = line.split(',')
                _sig  = int(_sig_str)
                _func = int(_func_str)
                _fanins = [int(_fanin_sig) for _fanin_sig in _fanin_str.split('|')]
                g = Primitive(_sig, _func, TYPES[_type_str], _fanins)
            items[g.sig] = g
    return items


def add_fanin_constraints(model: cp_model.CpModel, p, g, clocked : bool = False) -> None: 
    # Output clock stage should be >= input clock stage
    # model.Add( S[g.sig] >= S[p.sig] )
    # If output clock stage == input clock stage, then 
    #   output phase >= input phase (same stage is possible, if the gate is not clocked)
    
    S_eq = model.NewBoolVar(f'Same_S_{p.sig}_{g.sig}')
    model.Add(  S[g.sig] == S[p.sig]  ).OnlyEnforceIf( S_eq       )
    model.Add(  S[g.sig] >  S[p.sig]  ).OnlyEnforceIf( S_eq.Not() )
    
    if clocked:
        model.Add( Phi[g.sig] >  Phi[p.sig] ).OnlyEnforceIf( S_eq       )
    else:
        model.Add( Phi[g.sig] >= Phi[p.sig] ).OnlyEnforceIf( S_eq       )
    return S_eq
    # if clocked:
    #     model.Add( Phi[g.sig] >  Phi[p.sig] ).OnlyEnforceIf( S_eq       )
    #     # model.Add( Phi[g.sig] <= Phi[p.sig] ).OnlyEnforceIf( S_eq.Not() )
    # else:
    #     model.Add( Phi[g.sig] >= Phi[p.sig] ).OnlyEnforceIf( S_eq       )
    #     # model.Add( Phi[g.sig] <  Phi[p.sig] ).OnlyEnforceIf( S_eq.Not() )
    # Φɸφϕ𝛟
def 𝜑_le(model: cp_model.CpModel, a: Primitive, b: Primitive) -> cp_model.IntVar:
    Phi_cond = model.NewBoolVar( f"Phi_cond_{a.sig}_{b.sig}" )
    # If Phi_cond is True, you need one fewer DFF
    model.Add( Phi[a.sig] <= Phi[b.sig] ).OnlyEnforceIf( Phi_cond       )
    model.Add( Phi[a.sig] >  Phi[b.sig] ).OnlyEnforceIf( Phi_cond.Not() )
    return Phi_cond

def 𝜑_lt(model: cp_model.CpModel, a: Primitive, b: Primitive) -> cp_model.IntVar:
    Phi_cond = model.NewBoolVar( f"Phi_cond_{a.sig}_{b.sig}" )
    # If Phi_cond is True, you need one fewer DFF
    model.Add( Phi[a.sig] <  Phi[b.sig] ).OnlyEnforceIf( Phi_cond       )
    model.Add( Phi[a.sig] >= Phi[b.sig] ).OnlyEnforceIf( Phi_cond.Not() )
    return Phi_cond

def phi_lt(model: cp_model.CpModel, p: Primitive, g: Primitive) -> cp_model.IntVar:
    Phi_cond = model.NewBoolVar( f"Phi_cond_{p.sig}_{g.sig}" )
    # If Phi_cond is True, you need one fewer DFF
    model.Add( Phi[g.sig] <= Phi[p.sig] ).OnlyEnforceIf( Phi_cond       )
    model.Add( Phi[g.sig] >  Phi[p.sig] ).OnlyEnforceIf( Phi_cond.Not() )
    return Phi_cond

# Model.
model = cp_model.CpModel()

NPH = 2
S_bounds = ( 0, 1000 )         #Clock stage bounds
phi_bounds = 0, NPH - 1    #Phase bounds

# all_signals = parse_specs('/Users/brainkz/Documents/GitHub/mockturtle_alessandro/build/cavlc_specs.csv')
# all_signals = parse_specs('/Users/brainkz/Documents/GitHub/mockturtle_alessandro/build/c432_specs.csv')
all_signals = parse_specs('/Users/brainkz/Documents/GitHub/mockturtle_alessandro/build/c17_specs.csv')
# all_signals = parse_specs('/Users/brainkz/Documents/GitHub/mockturtle_alessandro/build/ctrl_specs.csv')

fanout = defaultdict(int)
S = {}
Phi = {}
fanin_constr = {}
for g in all_signals.values():
    Phi[g.sig] = model.NewIntVar(*phi_bounds, f'phi_{g.sig}')
    
    if g.type == 'PI':
        S[g.sig] = model.NewIntVar(0, 0, f'S_{g.sig}')   
    else:
        S[g.sig] = model.NewIntVar(*S_bounds, f'S_{g.sig}')    
        for p_sig in g.fanins:
            fanout[p_sig] += 1

fanin_diff = {}
Delta = {}
# IMPORTANT: PIs are assumed to be at clock stage 0 but with an arbitrary phase
expr = []
for g in all_signals.values():
    # print(f"The type of the gate {g.sig} is {g.type}")
    if g.type == 'AA':
        # continue
        # any PI is the earliest fanin, with zero phase and zero clock stage
        # IMPORTANT: only 2-input CB is supported
        assert(len(g.fanins) == 2)
        a_sig, b_sig = g.fanins
        a = all_signals[a_sig]
        b = all_signals[b_sig]
        
        # ILP Definition of absolute value |Sa - Sb|
        Delta[a.sig, b.sig] = model.NewIntVar(*S_bounds, f'D_{a.sig}_{b.sig}')
        model.Add( (S[a.sig] - S[b.sig]) <= Delta[a.sig, b.sig] )
        model.Add( (S[b.sig] - S[a.sig]) <= Delta[a.sig, b.sig] )
        expr.append( Delta[a.sig, b.sig] )
        
        # TODO : replace with a proper limit
        model.Add( NPH * S[g.sig] + Phi[g.sig] >= NPH * S[a.sig] + Phi[a.sig] )
        model.Add( NPH * S[g.sig] + Phi[g.sig] >= NPH * S[b.sig] + Phi[b.sig] )
        
        # define while avoiding constants
        # a_is_later = ( ( NPH*S[a.sig]+Phi[a.sig] ) > ( NPH*S[b.sig]+Phi[b.sig] ) )
        a_is_later = model.NewBoolVar(f'fanin_diff_{a_sig}_{b_sig}')
        model.Add( ( NPH*S[a.sig]+Phi[a.sig] ) >= ( NPH*S[b.sig]+Phi[b.sig] ) ).OnlyEnforceIf( a_is_later       )
        model.Add( ( NPH*S[a.sig]+Phi[a.sig] ) <  ( NPH*S[b.sig]+Phi[b.sig] ) ).OnlyEnforceIf( a_is_later.Not() )
        # Stage and phase equal to a, if a is the later one
        model.Add(   S[g.sig] ==   S[a.sig] ).OnlyEnforceIf( a_is_later       )
        model.Add( Phi[g.sig] == Phi[a.sig] ).OnlyEnforceIf( a_is_later       )
        # Stage and phase equal to b, if b is the later one
        model.Add(   S[g.sig] ==   S[b.sig] ).OnlyEnforceIf( a_is_later.Not() )
        model.Add( Phi[g.sig] == Phi[b.sig] ).OnlyEnforceIf( a_is_later.Not() )
        
        fanin_diff[a.sig, b.sig] = (a_is_later, )
        
        # Todo : make more accurate AA formulation
        
    # Regular gate - use the standard definition
    elif g.type == 'AS':
        for i, p_sig in enumerate(g.fanins):
            p = all_signals[p_sig]
            
            Phi_cond = 𝜑_le(model, g, p)
            fanin_constr[p.sig, g.sig] = add_fanin_constraints(model, p, g, clocked = True)
            
            if p.type == 'PI':
                # model.Add( Phi[g.sig] == 0 ).OnlyEnforceIf( Phi_cond       )
                # model.Add( Phi[g.sig] != 0 ).OnlyEnforceIf( Phi_cond.Not() )
                expr.append( S[g.sig] - Phi_cond ) 
                # expr.append( (S[g.sig] - 1).OnlyEnforceIf( Phi_cond ) )
                continue
            
            # Objective function is the clock stage difference between the fanin and fanout
            expr.append( S[g.sig] - S[p.sig] - Phi_cond ) 
            
    elif g.type == 'SA':
        
        assert(len(g.fanins) == 2)
        
        a_sig, b_sig = g.fanins
        a = all_signals[a_sig]
        b = all_signals[b_sig]
        
        a_is_true_AS = (a.type == 'AS') and (fanout[a.sig] == 1)
        b_is_true_AS = (b.type == 'AS') and (fanout[b.sig] == 1)
        
        if a_is_true_AS and b_is_true_AS:
            a_lt_b = model.NewBoolVar(f'{a.sig}_lt_{b.sig}')
            
            model.Add( NPH*S[a.sig]+Phi[a.sig] <  NPH*S[b.sig]+Phi[b.sig] ).OnlyEnforceIf(a_lt_b)
            # Place SA gate after b directly
            model.Add(   S[g.sig] ==   S[b.sig] ).OnlyEnforceIf(a_lt_b      )
            model.Add( Phi[g.sig] == Phi[b.sig] ).OnlyEnforceIf(a_lt_b      )
            𝜑b_le_𝜑a = 𝜑_le(model, b, a)
            
            model.Add( NPH*S[a.sig]+Phi[a.sig] >= NPH*S[b.sig]+Phi[b.sig] ).OnlyEnforceIf(a_lt_b.Not())
            # Place SA gate after a directly
            model.Add(   S[g.sig] ==   S[a.sig] ).OnlyEnforceIf(a_lt_b.Not())
            model.Add( Phi[g.sig] == Phi[a.sig] ).OnlyEnforceIf(a_lt_b.Not())
            𝜑a_le_𝜑b = 𝜑_le(model, a, b)
            
            # ILP Definition of absolute value |Sa - Sb|
            Delta[a.sig, b.sig] = model.NewIntVar(*S_bounds, f'D_{a.sig}_{b.sig}')
            model.Add( (S[a.sig] - S[b.sig]) <= Delta[a.sig, b.sig] )
            model.Add( (S[b.sig] - S[a.sig]) <= Delta[a.sig, b.sig] )
            
            cond_a = model.NewBoolVar(f"cond_a_{a.sig}_{b.sig}")
            model.AddBoolAnd(𝜑b_le_𝜑a, a_lt_b).OnlyEnforceIf(cond_a)
            
            cond_b = model.NewBoolVar(f"cond_b_{a.sig}_{b.sig}")
            model.AddBoolAnd(𝜑a_le_𝜑b, a_lt_b.Not()).OnlyEnforceIf(cond_b)

            expr.append( Delta[a.sig, b.sig] + 1 - cond_b - cond_a )
            
            fanin_diff[a.sig, b.sig] = (a_lt_b, cond_a, cond_b)
        
        if not a_is_true_AS and not b_is_true_AS:
            a_lt_b = model.NewBoolVar(f'{a.sig}_lt_{b.sig}')
            
            model.Add( NPH*S[a.sig]+Phi[a.sig] <  NPH*S[b.sig]+Phi[b.sig]     ).OnlyEnforceIf(a_lt_b      )
            # Place SA gate after b with DFF 
            model.Add( NPH*S[g.sig]+Phi[g.sig] == NPH*S[b.sig]+Phi[b.sig] + 1 ).OnlyEnforceIf(a_lt_b      )
            𝜑b_lt_𝜑a = 𝜑_lt(model, b, a)
            
            model.Add( NPH*S[a.sig]+Phi[a.sig] >= NPH*S[b.sig]+Phi[b.sig]     ).OnlyEnforceIf(a_lt_b.Not())
            # Place SA gate after a with DFF 
            model.Add( NPH*S[g.sig]+Phi[g.sig] == NPH*S[a.sig]+Phi[a.sig] + 1 ).OnlyEnforceIf(a_lt_b.Not())
            𝜑a_lt_𝜑b = 𝜑_lt(model, a, b)
            
            # ILP Definition of absolute value |Sa - Sb|
            Delta[a.sig, b.sig] = model.NewIntVar(*S_bounds, f'D_{a.sig}_{b.sig}')
            model.Add( (S[a.sig] - S[b.sig]) <= Delta[a.sig, b.sig] )
            model.Add( (S[b.sig] - S[a.sig]) <= Delta[a.sig, b.sig] )
            
            cond_a = model.NewBoolVar(f"cond_a_{a.sig}_{b.sig}")
            model.AddBoolAnd(𝜑b_lt_𝜑a, a_lt_b).OnlyEnforceIf(cond_a)
            model.AddBoolOr(𝜑b_lt_𝜑a.Not(), a_lt_b.Not()).OnlyEnforceIf(cond_a.Not())
            
            cond_b = model.NewBoolVar(f"cond_b_{a.sig}_{b.sig}")
            model.AddBoolAnd(𝜑a_lt_𝜑b, a_lt_b.Not()).OnlyEnforceIf(cond_b)
            model.AddBoolOr(𝜑a_lt_𝜑b.Not(), a_lt_b).OnlyEnforceIf(cond_b.Not())

            expr.append( Delta[a.sig, b.sig] + 2 - cond_a - cond_b )
        
            fanin_diff[a.sig, b.sig] = (a_lt_b, cond_a, cond_b)
            
        if not a_is_true_AS and b_is_true_AS:
            a, b = b, a
            a_is_true_AS, b_is_true_AS = b_is_true_AS, a_is_true_AS
            
        if a_is_true_AS and not b_is_true_AS:
            a_lt_b_p1 = model.NewBoolVar(f'{a.sig}_lt_{b.sig}_p1')
            model.Add( NPH*S[a.sig]+Phi[a.sig] <  NPH*S[b.sig]+Phi[b.sig]+1 ).OnlyEnforceIf(a_lt_b_p1)
            model.Add( NPH*S[a.sig]+Phi[a.sig] >= NPH*S[b.sig]+Phi[b.sig]+1 ).OnlyEnforceIf(a_lt_b_p1.Not())
            
            # Place SA gate after b with DFF 
            model.Add( NPH*S[g.sig]+Phi[g.sig] == NPH*S[b.sig]+Phi[b.sig]+1 ).OnlyEnforceIf(a_lt_b_p1)
            
            # Place SA gate after a directly
            model.Add(   S[g.sig] ==   S[a.sig] ).OnlyEnforceIf(a_lt_b_p1.Not())
            model.Add( Phi[g.sig] == Phi[a.sig] ).OnlyEnforceIf(a_lt_b_p1.Not())
            
            Phi_cond_a = phi_lt(model, a, b)
            Phi_cond_b = phi_lt(model, b, a)
            
            # ILP Definition of absolute value |Sa - Sb|
            Delta[a.sig, b.sig] = model.NewIntVar(*S_bounds, f'D_{a.sig}_{b.sig}')
            model.Add( (S[a.sig] - S[b.sig]) <= Delta[a.sig, b.sig] )
            model.Add( (S[b.sig] - S[a.sig]) <= Delta[a.sig, b.sig] )
            
            cond_a = model.NewBoolVar(f"cond_a_{a.sig}_{b.sig}")
            model.AddBoolAnd(Phi_cond_a, a_lt_b_p1.Not()).OnlyEnforceIf(cond_a)
            
            cond_b = model.NewBoolVar(f"cond_b_{a.sig}_{b.sig}")
            model.AddBoolAnd(Phi_cond_b, a_lt_b_p1).OnlyEnforceIf(cond_b)
            
            expr.append( Delta[a.sig, b.sig] - cond_a - cond_b + a_lt_b_p1 + 1)
        
            fanin_diff[a.sig, b.sig] = (a_lt_b_p1, cond_a, cond_b)
        
        
        """
        for i, p_sig in enumerate(g.fanins):
            p = all_signals[p_sig]
            
            Phi_cond = model.NewBoolVar( f"Phi_cond_{p.sig}_{g.sig}" )
            # If Phi_cond is True, you need one fewer DFF
            model.Add( Phi[g.sig] <= Phi[p.sig] ).OnlyEnforceIf( Phi_cond       )
            model.Add( Phi[g.sig] >  Phi[p.sig] ).OnlyEnforceIf( Phi_cond.Not() )
            
            fanin_constr[p.sig, g.sig] = add_fanin_constraints(model, p, g, clocked = False)
            # any PI is the earliest fanin, with zero phase and zero clock stage
            if p.type == 'PI':
                # model.Add( Phi[g.sig] == 0 ).OnlyEnforceIf( Phi_cond       )
                # model.Add( Phi[g.sig] != 0 ).OnlyEnforceIf( Phi_cond.Not() )
                expr.append( S[g.sig] + 1 - Phi_cond )
                continue
            
            # Objective function is the clock stage difference between the fanin and fanout
            # one extra DFF per fanin for simultaneous release
            expr.append( S[g.sig] - S[p.sig] + 1 - Phi_cond ) 
        """
            
model.Minimize(sum(expr))

# Solves and prints out the solution.
solver = cp_model.CpSolver()
status = solver.Solve(model)
print(f'Solve status: {solver.StatusName(status)}')
if status == cp_model.OPTIMAL:
    print('Optimal objective value: %i' % solver.ObjectiveValue())
print('Statistics')
print(f'  - conflicts : {solver.NumConflicts():d}')
print(f'  - branches  : {solver.NumBranches():d}')
print(f'  - wall time : {solver.WallTime():f} s')

for s, phi in zip(S.values(), Phi.values()):
    signal = int(s.Name()[2:])
    g = all_signals[signal]
    print(f'Sig {signal}:\t'
        f'{g.type}: [{",".join(str(p_sig) for p_sig in g.fanins)}]\t'
        f'{  s.Name()} = {solver.Value(  s)}\t'
        f'{phi.Name()} = {solver.Value(phi)}')
for (a_sig, b_sig), conds  in fanin_diff.items():
    print(f'\tcond: {a_sig, b_sig}: {[solver.Value(c) for c in conds]}')
for (a_sig, b_sig), delta  in Delta.items():
    print(f'\t∆ {a_sig, b_sig}: {solver.Value(delta)}')
    
    
    
