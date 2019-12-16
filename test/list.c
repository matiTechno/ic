struct lnode
{
    lnode* next;
    s32 data;
};

void add(lnode** it, s32 data)
{
    while (*it)
        it = &(*it)->next;

    *it = (lnode*)malloc(sizeof(lnode));
    (*it)->data = data;
}

void print(lnode* it)
{
    while (it)
    {
        printf(it->data);
        it = it->next;
    }
}

void reverse(lnode** head)
{
    lnode* prev = nullptr;
    lnode* it = *head;

    while (it)
    {
        lnode* next = it->next;
        it->next = prev;
        prev = it;
        it = next;
    }
    *head = prev;
}

s32 main()
{
    lnode* list = nullptr;
    add(&list, 1);
    reverse(&list);
    add(&list, 10);
    add(&list, 3);
    add(&list, 5);
    add(&list, 6);

    print(list);
    reverse(&list);
    prints("after reverse");
    print(list);
    return 0;
}