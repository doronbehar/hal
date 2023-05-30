import os
for m in netlist.get_modules():
    sys.stdout.write("Module:%d,%s,"% (m.get_id(),m.get_name()))
    pm = m.get_parent_module()
    if pm:
        sys.stdout.write("parent:%d,[Gates:"% (pm.get_id()))
    else:
        sys.stdout.write("no-parent,[Gates:")
    for g in m.get_gates():
        sys.stdout.write("%d,"% (g.get_id()))
    sys.stdout.write("Nets:,")
    for n in m.get_internal_nets():
        sys.stdout.write("%d,"% (n.get_id()))
    sys.stdout.write("]\n")
for g in netlist.get_gates():
    sys.stdout.write("Gate:%d,%s,%d\n"% (g.get_id(),g.get_name(),g.get_module().get_id()))
for n in netlist.get_nets():
    sys.stdout.write("Net:%d,%s\n"% (n.get_id(),n.get_name()))