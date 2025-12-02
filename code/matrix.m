clc;
M = 100;                        % Define the size of the matrix
rng(0);                        % for reproducibility
A = randi([-9,9],M,M);         % MxM integer matrix
W = randi([-9,9],M,M);         % MxM integer matrix
C = A * W;                     % MxM product

% Helper to print in the requested C array format (row-major)
printCArray = @(name, M) ...
    fprintf(['float %s[%d] = {\n' ...
             repmat('            ',1,1)], name, M*M); %#ok<PRFLG>

% Print A
fprintf('float A1[%d] = {\n', M*M);
for r = 1:M
    fprintf('            ');
    for c = 1:M
        v = double(A(r,c));
        if v == floor(v)
            fprintf('%.1f', v);
        else
            fprintf('%.6g', v);
        end
        if ~(r==M && c==M)
            fprintf(', ');
        end
    end
    fprintf('\n');
end
fprintf('        };\n\n');

% Print W
fprintf('float W1[%d] = {\n', M*M);
for r = 1:M
    fprintf('            ');
    for c = 1:M
        v = double(W(r,c));
        if v == floor(v)
            fprintf('%.1f', v);
        else
            fprintf('%.6g', v);
        end
        if ~(r==M && c==M)
            fprintf(', ');
        end
    end
    fprintf('\n');
end
fprintf('        };\n\n');

% Print C = A1 * W1
fprintf('float C1[%d] = {\n', M*M);
for r = 1:M
    fprintf('            ');
    for c = 1:M
        v = double(C(r,c));
        if v == floor(v)
            fprintf('%.1f', v);
        else
            fprintf('%.6g', v);
        end
        if ~(r==M && c==M)
            fprintf(', ');
        end
    end
    fprintf('\n');
end
fprintf('        };\n');
