

def splitListForNodes(list, nodes):
    
    rows = len(list)           # number of elements in list
    max_items_per_node = rows / nodes
    rest = rows % nodes
    
    splitted_list = []  
    
    start = 0
    for node in range(nodes):
            
        # start und end der teilliste bestimmen        
        end = start + max_items_per_node  
            
        # because the max number of parameter cannot be equally
        # distributed among the cores, some cores must deal
        # with one additional parameter
        if (node<rest):
            end = end+1
            
        # Assert that end is not larger than list size
        if (end>rows):
            end = rows
    
        # Teilliste als Element in neue Liste einfuegen
        splitted_list.append(list[start:end])
            
        # remeber this end because this is the starting point
        # for next round
        start = end
        
    return splitted_list