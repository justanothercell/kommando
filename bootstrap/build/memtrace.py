with open('MEMTRACE.txt', 'r') as tracefile:
    traces = tracefile.readlines()

memory = {}
total_memory = {}
total = 0

for trace in traces:
    try:
        kind, data = trace.split(' ', 1)
        if kind == 'MALLOC':
            data, loc = data.split('|')
            addr, size = data.split(' ', 1)
            if addr == '0000000000000000':
                raise Exception(f'invalid alloc (null): `{trace}` of {addr}')
            elif addr in memory:
                raise Exception(f'invalid alloc (already in use): `{trace}` of {addr}')
            memory[addr.strip()] = trace
            total_memory[addr.strip()] = trace
            total += 1
        elif kind == 'FREE':
            addr, loc = data.rsplit('|', 1)
            addr = addr.strip()
            if addr != '0000000000000000':
                if addr not in memory:
                    if addr_in in total_memory:
                        raise Exception(f'invalid free (already freed): `{trace}` of {addr}')
                    else:
                        raise Exception(f'invalid free (not allocated): `{trace}` of {addr}')
                del memory[addr]
        elif kind == 'REALLOC': # REALLOC tok @ 0000017688AB1AE0 with (tok_cap) * sizeof('\0') (72) -> @ 0000017688AAF730 | tokens.c:134
            data, loc = data.split('|')
            addr_in, addr_out_and_size = data.split(' to ')
            addr_out, size = addr_out_and_size.strip().split(' ', 1)
            addr_in = addr_in.strip()
            addr_out = addr_out.strip()
            if addr_in != '0000000000000000':
                if addr_in not in memory:
                    if addr_in in total_memory:
                        raise Exception(f'invalid realloc (already freed): `{trace}` from {addr_in} to {addr_out}')
                    else:
                        raise Exception(f'invalid realloc (not allocated): `{trace}` from {addr_in} to {addr_out}')
                del memory[addr_in]
                total -= 1
            if addr_out == '0000000000000000':
                raise Exception(f'invalid realloc (null): `{trace}` of {addr_out}')
            elif addr_out in memory:
                raise Exception(f'invalid realloc (already in use): `{trace}` of {addr_out}')
            memory[addr_out] = trace
            total_memory[addr_out] = trace
            total += 1
        else:
            raise Exception(f'invalid trace data: `{trace}`')
    except ValueError:
        raise Exception(f'invalid trace data: `{trace}`')

print(f'Ended with {len(memory)} unfreed heap values ({len(memory)*100/total:.2f}%)')

#for v in memory.values():
#    print(v)