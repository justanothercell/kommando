import sys

with open('MEMTRACE.txt', 'r') as tracefile:
    traces = tracefile.readlines()

input_ptrs = sys.argv[1:]

memory = {}
total_memory = {}
total = 0

for trace in traces:
    try:
        kind, rest = trace.split(' ', 1)
        data, loc = rest.split('@')
        if kind == 'MALLOC':
            ptr, size = data.strip().split(' ')
            if ptr in input_ptrs:
                print(f'found {ptr}:', trace.strip())
            if ptr == '(nil)':
                raise Exception(f'invalid alloc (null): {ptr} of size {size} @ {loc}')
            elif ptr in memory:
                raise Exception(f'invalid alloc (already in use): {ptr} of size {size} @ {loc}')
            memory[ptr] = trace
            total_memory[ptr] = trace
            total += 1
        elif kind == 'FREE':
            ptr = data.strip()
            if ptr in input_ptrs:
                print(f'found {ptr}:', trace.strip())
            if ptr != '(nil)':
                if ptr not in memory:
                    if ptr in total_memory:
                        raise Exception(f'invalid free (already freed): {ptr} @ {loc}')
                    else:
                        raise Exception(f'invalid free (not allocated): {ptr} @ {loc}')
                del memory[ptr]
        elif kind == 'REALLOC':
            ptr, new_ptr, size = data.strip().split(' ')
            if ptr in input_ptrs:
                print(f'found {ptr}:', trace.strip())
            if new_ptr in input_ptrs:
                print(f'found {new_ptr}:', trace.strip())
            if ptr != '(nil)':
                if ptr not in memory:
                    if ptr in total_memory:
                        raise Exception(f'invalid realloc (already freed): {ptr} to {new_ptr} of size {size}')
                    else:
                        raise Exception(f'invalid realloc (not allocated): {ptr} to {new_ptr} of size {size}')
                del memory[ptr]
                total -= 1
            if new_ptr == '(nil)':
                raise Exception(f'invalid realloc (null): {ptr} to {new_ptr} of size {size}')
            elif new_ptr in memory:
                raise Exception(f'invalid realloc (already in use): {ptr} to {new_ptr} of size {size}')
            memory[new_ptr] = trace
            total_memory[new_ptr] = trace
            total += 1
        else:
            raise Exception(f'invalid trace data: `{trace}`')
    except ValueError:
        raise Exception(f'invalid trace data: `{trace}`')

print(f'Ended with {len(memory)} unfreed heap values ({len(memory)*100/total:.2f}%)')

#for v in memory.values():
#    print(v)