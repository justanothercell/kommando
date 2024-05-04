def find_interval(a: list, b: list) -> (int, int):
    max_size = 0
    max_bounds = 0, 0
    n = len(a) # len(a) == len(b)
    for i in range(n): # [0, 1, ..., n - 1]
        for j in range(i, n+1): # [i, ..., n - 1]
            if j - i <= max_size:
                continue # we only care for bigger bounds than the already found one
            if sum(a[i:j]) == sum(b[i:j]):
                max_size = j - i
                max_bounds = i, j
    return max_bounds

def find_interval(a: list, b: list) -> (int, int):
    max_size = 0
    max_bounds = 0, 0
    m = set()
    n = len(a) # len(a) == len(b)
    for i in range(n): # [0, 1, ..., n - 1]
        for j in range(i, n+1): # [i, ..., n - 1]
            if j - i <= max_size:
                continue # we only care for bigger bounds than the already found one
            if sum(a[i:j]) == sum(b[i:j]):
                max_size = j - i
                max_bounds = i, j
    return max_bounds

print(find_interval([1, 2, 5, -3, 5, 0, 6], [-1, 3, 4, 0, 1, 2, 3]))
print(find_interval2([1, 2, 5, -3, 5, 0, 6], [-1, 3, 4, 0, 1, 2, 3]))


