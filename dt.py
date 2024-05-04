def convert(num: str, from_base: int, to_base: int, precicion=16) -> str:
    DIGITS = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F']
    num = num.strip().upper().replace(',', '.').replace('_', '')
    if '.' in num:
        i, f = num.split('.')
    else:
        i, f = num, ''
    ipart = 0
    for c, digit in enumerate(i[::-1]):
        ipart += from_base ** c * DIGITS.index(digit)
    fpart_num = 0
    fpart_de = from_base ** len(f)
    for c, digit in enumerate(f[::-1]):
        fpart_num += from_base ** c * DIGITS.index(digit)
    new_num = ''
    if ipart == 0:
        new_num += '0'
    while ipart > 0:
        new_num = DIGITS[ipart % to_base] + new_num
        ipart //= to_base
    if fpart_de > 1:
        new_num += '.'
    while precicion > 0:
        fpart_num *= to_base
        f = fpart_num // fpart_de % to_base
        fpart_num -= f * fpart_de
        new_num += DIGITS[f]
        precicion -= 1
    return new_num.rstrip('0')


print(convert('B2,7A', 16, 10))
