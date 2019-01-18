int main() {
    int i = 0;
    do {
        if (i < 2) {
            i += 1;
            continue;
        }
    } while (0);

    return i == 2;
}
